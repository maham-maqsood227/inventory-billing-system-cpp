// ============================================================
//  Inventory & Billing System  (single-file version)
//  Compile:  g++ -std=c++17 inventory_billing.cpp -o inventory
//  Run    :  ./inventory      (Windows: inventory.exe)
//  Admin password: admin123
// ============================================================
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

// ------------------------------------------------------------
// Settings (files are created in the folder you run from)
// ------------------------------------------------------------
const string ADMIN_PASSWORD = "admin123";   // demo only - not secure
const string PRODUCTS_FILE  = "products.csv";
const string SALES_FILE     = "sales.csv";
const string ITEMS_FILE     = "bill_items.csv";

// ------------------------------------------------------------
// Utils
// ------------------------------------------------------------
namespace Utils {

string readLine(const string& prompt) {
    string line;
    cout << prompt;
    if (!getline(cin, line)) throw runtime_error("Input closed");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

int readInt(const string& prompt, int minVal, int maxVal) {
    while (true) {
        string line = readLine(prompt);
        try {
            size_t pos = 0;
            int value = stoi(line, &pos);
            if (pos == line.size() && value >= minVal && value <= maxVal) return value;
        } catch (const exception&) {}
        cout << "  Invalid input. Enter a whole number between "
             << minVal << " and " << maxVal << ".\n";
    }
}

double readDouble(const string& prompt, double minVal, double maxVal = 1e9) {
    while (true) {
        string line = readLine(prompt);
        try {
            size_t pos = 0;
            double value = stod(line, &pos);
            if (pos == line.size() && value >= minVal && value <= maxVal) return value;
        } catch (const exception&) {}
        cout << "  Invalid input. Enter a valid number";
        if (maxVal < 1e9) cout << " between " << minVal << " and " << maxVal;
        else cout << " (minimum " << minVal << ")";
        cout << ".\n";
    }
}

string today() {
    time_t t = time(nullptr);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", localtime(&t));
    return buf;
}

vector<string> split(const string& s, char delim) {
    vector<string> parts;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) parts.push_back(item);
    return parts;
}

string trimCR(string s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

} // namespace Utils

// ------------------------------------------------------------
// Product
// ------------------------------------------------------------
class Product {
private:
    int id;
    string name;
    double price;
    int quantity;
    int reorderLevel;

public:
    Product(int id, const string& name, double price, int quantity, int reorderLevel)
        : id(id), price(0), quantity(0), reorderLevel(reorderLevel) {
        if (reorderLevel < 0) throw invalid_argument("Reorder level cannot be negative");
        setName(name);
        setPrice(price);
        setQuantity(quantity);
    }

    int getId() const { return id; }
    const string& getName() const { return name; }
    double getPrice() const { return price; }
    int getQuantity() const { return quantity; }
    int getReorderLevel() const { return reorderLevel; }

    void setName(const string& newName) {
        if (newName.empty()) throw invalid_argument("Product name cannot be empty");
        if (newName.find(',') != string::npos)
            throw invalid_argument("Product name cannot contain commas");
        name = newName;
    }
    void setPrice(double newPrice) {
        if (newPrice < 0) throw invalid_argument("Price cannot be negative");
        price = newPrice;
    }
    void setQuantity(int newQty) {
        if (newQty < 0) throw invalid_argument("Quantity cannot be negative");
        quantity = newQty;
    }

    bool isLowStock() const { return quantity <= reorderLevel; }

    string toCSV() const {
        return to_string(id) + "," + name + "," + to_string(price) + "," +
               to_string(quantity) + "," + to_string(reorderLevel);
    }

    static Product fromCSV(const string& line) {
        vector<string> f = Utils::split(line, ',');
        if (f.size() != 5) throw invalid_argument("Bad product record: " + line);
        return Product(stoi(f[0]), f[1], stod(f[2]), stoi(f[3]), stoi(f[4]));
    }
};

// ------------------------------------------------------------
// Inventory
// ------------------------------------------------------------
class Inventory {
private:
    map<int, Product> products;
    int nextId;

    static string toLower(string s) {
        transform(s.begin(), s.end(), s.begin(),
                  [](unsigned char c) { return static_cast<char>(tolower(c)); });
        return s;
    }

public:
    Inventory() : nextId(1) {}

    int addProduct(const string& name, double price, int qty, int reorderLevel) {
        Product p(nextId, name, price, qty, reorderLevel);   // validates input
        products.insert(make_pair(nextId, p));
        return nextId++;
    }

    void addLoadedProduct(const Product& p) {
        products.insert(make_pair(p.getId(), p));
        nextId = max(nextId, p.getId() + 1);
    }

    bool removeProduct(int id) { return products.erase(id) > 0; }

    Product* findById(int id) {
        map<int, Product>::iterator it = products.find(id);
        return it == products.end() ? nullptr : &it->second;
    }

    vector<const Product*> searchByName(const string& keyword) const {
        vector<const Product*> result;
        string key = toLower(keyword);
        for (map<int, Product>::const_iterator it = products.begin(); it != products.end(); ++it)
            if (toLower(it->second.getName()).find(key) != string::npos)
                result.push_back(&it->second);
        return result;
    }

    vector<const Product*> lowStockProducts() const {
        vector<const Product*> result;
        for (map<int, Product>::const_iterator it = products.begin(); it != products.end(); ++it)
            if (it->second.isLowStock()) result.push_back(&it->second);
        return result;
    }

    const map<int, Product>& all() const { return products; }

    void restock(int id, int amount) {
        Product* p = findById(id);
        if (!p) throw runtime_error("Product not found");
        if (amount <= 0) throw invalid_argument("Restock amount must be positive");
        p->setQuantity(p->getQuantity() + amount);
    }

    void reduceStock(int id, int amount) {
        Product* p = findById(id);
        if (!p) throw runtime_error("Product not found");
        if (amount > p->getQuantity())
            throw runtime_error("Not enough stock for " + p->getName() +
                                " (available: " + to_string(p->getQuantity()) + ")");
        p->setQuantity(p->getQuantity() - amount);
    }

    void display() const {
        cout << left << setw(6) << "ID" << setw(22) << "Name"
             << right << setw(10) << "Price" << setw(8) << "Qty" << setw(10) << "Reorder" << "\n";
        cout << string(56, '-') << "\n";
        for (map<int, Product>::const_iterator it = products.begin(); it != products.end(); ++it) {
            const Product& p = it->second;
            cout << left << setw(6) << p.getId() << setw(22) << p.getName()
                 << right << setw(10) << fixed << setprecision(2) << p.getPrice()
                 << setw(8) << p.getQuantity() << setw(10) << p.getReorderLevel()
                 << (p.isLowStock() ? "  LOW" : "") << "\n";
        }
    }

    bool empty() const { return products.empty(); }
};

// ------------------------------------------------------------
// Discounts (polymorphism)
// ------------------------------------------------------------
class DiscountPolicy {
public:
    virtual ~DiscountPolicy() {}
    virtual double calculate(double subtotal) const = 0;
    virtual string description() const = 0;
};

class NoDiscount : public DiscountPolicy {
public:
    double calculate(double) const override { return 0.0; }
    string description() const override { return "No discount"; }
};

class PercentDiscount : public DiscountPolicy {
    double percent;
public:
    explicit PercentDiscount(double p) : percent(p) {}
    double calculate(double subtotal) const override { return subtotal * percent / 100.0; }
    string description() const override {
        return "Discount (" + to_string(static_cast<int>(percent)) + "%)";
    }
};

class FlatDiscount : public DiscountPolicy {
    double amount;
public:
    explicit FlatDiscount(double a) : amount(a) {}
    double calculate(double subtotal) const override { return amount > subtotal ? subtotal : amount; }
    string description() const override { return "Flat discount"; }
};

// ------------------------------------------------------------
// BillItem and Bill
// ------------------------------------------------------------
struct BillItem {
    int productId;
    string name;
    int quantity;
    double unitPrice;

    double lineTotal() const { return quantity * unitPrice; }
};

class Bill {
private:
    int billId;
    string date;
    vector<BillItem> items;
    unique_ptr<DiscountPolicy> discount;

public:
    Bill(int id, const string& date)
        : billId(id), date(date), discount(new NoDiscount()) {}

    void addItem(const BillItem& item) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].productId == item.productId) {
                items[i].quantity += item.quantity;   // merge duplicate lines
                return;
            }
        }
        items.push_back(item);
    }

    void setDiscount(unique_ptr<DiscountPolicy> policy) { discount = move(policy); }

    int getId() const { return billId; }
    const string& getDate() const { return date; }
    const vector<BillItem>& getItems() const { return items; }
    bool empty() const { return items.empty(); }

    double subtotal() const {
        double sum = 0;
        for (size_t i = 0; i < items.size(); ++i) sum += items[i].lineTotal();
        return sum;
    }
    double discountAmount() const { return discount->calculate(subtotal()); }
    double total() const { return subtotal() - discountAmount(); }

    void print() const {
        cout << "\n========== INVOICE #" << billId << " ==========\n";
        cout << "Date: " << date << "\n";
        cout << string(40, '-') << "\n";
        cout << left << setw(16) << "Item" << right << setw(5) << "Qty"
             << setw(10) << "Price" << setw(10) << "Total" << "\n";
        cout << string(40, '-') << "\n";
        cout << fixed << setprecision(2);
        for (size_t i = 0; i < items.size(); ++i) {
            cout << left << setw(16) << items[i].name.substr(0, 15) << right
                 << setw(5) << items[i].quantity << setw(10) << items[i].unitPrice
                 << setw(10) << items[i].lineTotal() << "\n";
        }
        cout << string(40, '-') << "\n";
        double disc = discountAmount();
        cout << left << setw(26) << "Subtotal:" << right << setw(14) << subtotal() << "\n";
        cout << left << setw(26) << discount->description() << right
             << setw(14) << (disc > 0 ? -disc : 0.0) << "\n";
        cout << left << setw(26) << "TOTAL:" << right << setw(14) << total() << "\n";
        cout << string(40, '=') << "\n";
    }
};

// ------------------------------------------------------------
// FileManager (CSV storage)
// ------------------------------------------------------------
class FileManager {
public:
    void saveProducts(const Inventory& inv) const {
        ofstream out(PRODUCTS_FILE.c_str());
        if (!out) throw runtime_error("Cannot write " + PRODUCTS_FILE);
        const map<int, Product>& all = inv.all();
        for (map<int, Product>::const_iterator it = all.begin(); it != all.end(); ++it)
            out << it->second.toCSV() << "\n";
    }

    // returns true if a products file was found
    bool loadProducts(Inventory& inv) const {
        ifstream in(PRODUCTS_FILE.c_str());
        if (!in) return false;
        string line;
        while (getline(in, line)) {
            line = Utils::trimCR(line);
            if (line.empty()) continue;
            try {
                inv.addLoadedProduct(Product::fromCSV(line));
            } catch (const exception& e) {
                cerr << "Skipping bad line in products file: " << e.what() << "\n";
            }
        }
        return true;
    }

    int nextBillId() const {
        ifstream in(SALES_FILE.c_str());
        int maxId = 0;
        string line;
        while (getline(in, line)) {
            vector<string> f = Utils::split(Utils::trimCR(line), ',');
            if (!f.empty()) {
                try { maxId = max(maxId, stoi(f[0])); } catch (...) {}
            }
        }
        return maxId + 1;
    }

    void saveBill(const Bill& bill) const {
        ofstream sales(SALES_FILE.c_str(), ios::app);
        ofstream items(ITEMS_FILE.c_str(), ios::app);
        if (!sales || !items) throw runtime_error("Cannot write sales files");
        sales << bill.getId() << "," << bill.getDate() << "," << fixed << setprecision(2)
              << bill.total() << "," << bill.discountAmount() << "\n";
        const vector<BillItem>& list = bill.getItems();
        for (size_t i = 0; i < list.size(); ++i) {
            items << bill.getId() << "," << list[i].productId << "," << list[i].name << ","
                  << list[i].quantity << "," << fixed << setprecision(2)
                  << list[i].lineTotal() << "\n";
        }
    }

    void printDailyReport(const string& date) const {
        ifstream in(SALES_FILE.c_str());
        int bills = 0;
        double revenue = 0, discounts = 0;
        string line;
        while (getline(in, line)) {
            vector<string> f = Utils::split(Utils::trimCR(line), ',');
            if (f.size() == 4 && f[1] == date) {
                try {
                    revenue += stod(f[2]);
                    discounts += stod(f[3]);
                    ++bills;
                } catch (...) {}
            }
        }
        cout << "\n===== Sales Report for " << date << " =====\n";
        cout << fixed << setprecision(2);
        cout << "Bills issued   : " << bills << "\n";
        cout << "Total revenue  : " << revenue << "\n";
        cout << "Discounts given: " << discounts << "\n";
        if (bills > 0) cout << "Average bill   : " << revenue / bills << "\n";
    }

    void printBestSellers(int topN) const {
        ifstream in(ITEMS_FILE.c_str());
        map<string, int> sold;
        string line;
        while (getline(in, line)) {
            vector<string> f = Utils::split(Utils::trimCR(line), ',');
            if (f.size() == 5) {
                try { sold[f[2]] += stoi(f[3]); } catch (...) {}
            }
        }
        vector<pair<string, int> > ranked(sold.begin(), sold.end());
        sort(ranked.begin(), ranked.end(),
             [](const pair<string, int>& a, const pair<string, int>& b) { return a.second > b.second; });
        cout << "\n===== Best Sellers =====\n";
        if (ranked.empty()) { cout << "No sales yet.\n"; return; }
        for (int i = 0; i < topN && i < static_cast<int>(ranked.size()); ++i)
            cout << i + 1 << ". " << ranked[i].first << " - " << ranked[i].second << " sold\n";
    }
};

// ------------------------------------------------------------
// Menu actions
// ------------------------------------------------------------
void addProductMenu(Inventory& inv) {
    string name = Utils::readLine("Product name: ");
    double price = Utils::readDouble("Price: ", 0);
    int qty = Utils::readInt("Quantity in stock: ", 0, 1000000);
    int reorder = Utils::readInt("Reorder level (alert when stock <= this): ", 0, 1000000);
    int id = inv.addProduct(name, price, qty, reorder);
    cout << "Product added with ID " << id << ".\n";
}

void updateProductMenu(Inventory& inv) {
    int id = Utils::readInt("Product ID to update: ", 1, 1000000);
    Product* p = inv.findById(id);
    if (!p) { cout << "Product not found.\n"; return; }
    cout << "Updating " << p->getName() << "\n";
    cout << " 1. Change price\n 2. Restock (add quantity)\n 3. Rename\n";
    int choice = Utils::readInt("Choice: ", 1, 3);
    if (choice == 1) p->setPrice(Utils::readDouble("New price: ", 0));
    else if (choice == 2) inv.restock(id, Utils::readInt("Quantity to add: ", 1, 1000000));
    else p->setName(Utils::readLine("New name: "));
    cout << "Updated.\n";
}

void deleteProductMenu(Inventory& inv) {
    int id = Utils::readInt("Product ID to delete: ", 1, 1000000);
    cout << (inv.removeProduct(id) ? "Product deleted.\n" : "Product not found.\n");
}

void searchMenu(const Inventory& inv) {
    string key = Utils::readLine("Search keyword: ");
    vector<const Product*> results = inv.searchByName(key);
    if (results.empty()) { cout << "No matches.\n"; return; }
    for (size_t i = 0; i < results.size(); ++i)
        cout << results[i]->getId() << " | " << results[i]->getName() << " | "
             << results[i]->getPrice() << " | qty " << results[i]->getQuantity() << "\n";
}

void lowStockMenu(const Inventory& inv) {
    vector<const Product*> low = inv.lowStockProducts();
    if (low.empty()) { cout << "All products are well stocked.\n"; return; }
    cout << "Products that need restocking:\n";
    for (size_t i = 0; i < low.size(); ++i)
        cout << " - " << low[i]->getName() << " (qty " << low[i]->getQuantity()
             << ", reorder level " << low[i]->getReorderLevel() << ")\n";
}

void createBillMenu(Inventory& inv, const FileManager& fm) {
    if (inv.empty()) { cout << "No products available.\n"; return; }
    Bill bill(fm.nextBillId(), Utils::today());

    while (true) {
        int id = Utils::readInt("Product ID (0 to finish): ", 0, 1000000);
        if (id == 0) break;
        Product* p = inv.findById(id);
        if (!p) { cout << "  Product not found.\n"; continue; }
        int qty = Utils::readInt("  Quantity: ", 1, 1000000);
        try {
            inv.reduceStock(id, qty);            // throws if not enough stock
            BillItem item = { p->getId(), p->getName(), qty, p->getPrice() };
            bill.addItem(item);
            cout << "  Added " << qty << " x " << p->getName() << "\n";
        } catch (const exception& e) {
            cout << "  " << e.what() << "\n";
        }
    }

    if (bill.empty()) { cout << "Empty bill cancelled.\n"; return; }

    cout << "Discount: 1) None  2) Percent  3) Flat amount\n";
    int d = Utils::readInt("Choice: ", 1, 3);
    if (d == 2)
        bill.setDiscount(unique_ptr<DiscountPolicy>(new PercentDiscount(Utils::readDouble("Percent (0-100): ", 0, 100))));
    else if (d == 3)
        bill.setDiscount(unique_ptr<DiscountPolicy>(new FlatDiscount(Utils::readDouble("Amount: ", 0))));

    bill.print();
    fm.saveBill(bill);
    fm.saveProducts(inv);
    cout << "Bill saved.\n";
}

bool login() {
    for (int attempt = 1; attempt <= 3; ++attempt) {
        if (Utils::readLine("Admin password: ") == ADMIN_PASSWORD) return true;
        cout << "Wrong password (" << attempt << "/3)\n";
    }
    return false;
}

void printMenu() {
    cout << "\n===== INVENTORY & BILLING SYSTEM =====\n"
         << " 1. View all products\n"
         << " 2. Add product\n"
         << " 3. Update product\n"
         << " 4. Delete product\n"
         << " 5. Search product\n"
         << " 6. Low-stock alerts\n"
         << " 7. Create bill\n"
         << " 8. Daily sales report\n"
         << " 9. Best-selling products\n"
         << " 0. Save and exit\n";
}

// ------------------------------------------------------------
// main
// ------------------------------------------------------------
int main() {
    Inventory inventory;
    FileManager files;

    // First run: no products file yet, so start with some sample products
    if (!files.loadProducts(inventory)) {
        inventory.addProduct("Rice 5kg", 950, 50, 10);
        inventory.addProduct("Sugar 1kg", 160, 8, 10);
        inventory.addProduct("Cooking Oil 1L", 520, 30, 10);
        inventory.addProduct("Tea 500g", 780, 20, 5);
        inventory.addProduct("Milk Pack", 240, 6, 10);
        files.saveProducts(inventory);
    }

    try {
        cout << "Welcome! Please log in.\n";
        if (!login()) { cout << "Too many failed attempts.\n"; return 1; }

        while (true) {
            printMenu();
            int choice = Utils::readInt("Choice: ", 0, 9);
            try {
                switch (choice) {
                    case 1: inventory.display(); break;
                    case 2: addProductMenu(inventory); files.saveProducts(inventory); break;
                    case 3: updateProductMenu(inventory); files.saveProducts(inventory); break;
                    case 4: deleteProductMenu(inventory); files.saveProducts(inventory); break;
                    case 5: searchMenu(inventory); break;
                    case 6: lowStockMenu(inventory); break;
                    case 7: createBillMenu(inventory, files); break;
                    case 8: files.printDailyReport(Utils::today()); break;
                    case 9: files.printBestSellers(5); break;
                    case 0: files.saveProducts(inventory); cout << "Goodbye!\n"; return 0;
                }
            } catch (const exception& e) {
                cout << "Error: " << e.what() << "\n";   // one bad action won't crash the app
            }
        }
    } catch (const exception&) {
        // input stream closed (Ctrl+Z / Ctrl+D): save and quit
        try { files.saveProducts(inventory); } catch (...) {}
    }
    return 0;
}