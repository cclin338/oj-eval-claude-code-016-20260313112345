#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int MAX_KEY_SIZE = 65;
const int ORDER = 100;

struct Key {
    char str[MAX_KEY_SIZE];

    Key() { memset(str, 0, sizeof(str)); }

    Key(const string& s) {
        memset(str, 0, sizeof(str));
        strncpy(str, s.c_str(), MAX_KEY_SIZE - 1);
    }

    int compare(const Key& other) const {
        return strcmp(str, other.str);
    }

    bool operator<(const Key& other) const {
        return compare(other) < 0;
    }

    bool operator==(const Key& other) const {
        return compare(other) == 0;
    }

    bool operator<=(const Key& other) const {
        return compare(other) <= 0;
    }
};

struct KeyValue {
    Key key;
    int value;

    bool operator<(const KeyValue& other) const {
        int cmp = key.compare(other.key);
        if (cmp < 0) return true;
        if (cmp > 0) return false;
        return value < other.value;
    }

    bool operator==(const KeyValue& other) const {
        return key == other.key && value == other.value;
    }
};

class BPlusTree {
private:
    struct Node {
        bool is_leaf;
        int n;
        KeyValue keys[ORDER];
        int children[ORDER + 1];
        int next;

        Node() : is_leaf(true), n(0), next(-1) {
            for (int i = 0; i <= ORDER; i++) {
                children[i] = -1;
            }
        }
    };

    fstream file;
    string filename;
    int root_pos;
    int node_count;

    void init_file() {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file.is_open() || file.peek() == EOF) {
            if (file.is_open()) file.close();
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);

            root_pos = 0;
            node_count = 1;
            Node root;
            write_node(0, root);
            write_header();
        } else {
            read_header();
        }
    }

    void read_header() {
        file.seekg(0, ios::beg);
        file.read((char*)&root_pos, sizeof(root_pos));
        file.read((char*)&node_count, sizeof(node_count));
    }

    void write_header() {
        file.seekp(0, ios::beg);
        file.write((char*)&root_pos, sizeof(root_pos));
        file.write((char*)&node_count, sizeof(node_count));
        file.flush();
    }

    int get_node_offset(int pos) {
        return sizeof(int) * 2 + pos * sizeof(Node);
    }

    void read_node(int pos, Node& node) {
        file.seekg(get_node_offset(pos), ios::beg);
        file.read((char*)&node, sizeof(Node));
    }

    void write_node(int pos, const Node& node) {
        file.seekp(get_node_offset(pos), ios::beg);
        file.write((char*)&node, sizeof(Node));
        file.flush();
    }

    int allocate_node() {
        return node_count++;
    }

    void split_child(Node& parent, int idx, int child_pos) {
        Node child;
        read_node(child_pos, child);

        Node new_node;
        new_node.is_leaf = child.is_leaf;

        int mid = ORDER / 2;
        new_node.n = child.n - mid;

        for (int i = 0; i < new_node.n; i++) {
            new_node.keys[i] = child.keys[mid + i];
        }

        if (child.is_leaf) {
            new_node.next = child.next;
            child.next = node_count;
            child.n = mid;
        } else {
            for (int i = 0; i <= new_node.n; i++) {
                new_node.children[i] = child.children[mid + i];
            }
            child.n = mid;
        }

        int new_pos = allocate_node();
        write_node(new_pos, new_node);
        write_node(child_pos, child);

        for (int i = parent.n; i > idx; i--) {
            parent.children[i + 1] = parent.children[i];
            parent.keys[i] = parent.keys[i - 1];
        }

        parent.children[idx + 1] = new_pos;
        parent.keys[idx] = new_node.keys[0];
        parent.n++;
    }

    void insert_non_full(int pos, const KeyValue& kv) {
        Node node;
        read_node(pos, node);

        if (node.is_leaf) {
            int i = node.n - 1;
            while (i >= 0 && kv < node.keys[i]) {
                node.keys[i + 1] = node.keys[i];
                i--;
            }
            node.keys[i + 1] = kv;
            node.n++;
            write_node(pos, node);
        } else {
            int i = node.n - 1;
            while (i >= 0 && kv.key < node.keys[i].key) {
                i--;
            }
            i++;

            Node child;
            read_node(node.children[i], child);
            if (child.n == ORDER) {
                split_child(node, i, node.children[i]);
                write_node(pos, node);
                read_node(pos, node);

                if (!(kv.key < node.keys[i].key)) {
                    i++;
                }
            }
            insert_non_full(node.children[i], kv);
        }
    }

    int find_leaf(const Key& key) {
        int pos = root_pos;
        Node node;
        read_node(pos, node);

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && !(key < node.keys[i].key)) {
                i++;
            }
            pos = node.children[i];
            read_node(pos, node);
        }

        return pos;
    }

    void remove_from_leaf(int pos, const KeyValue& kv) {
        Node node;
        read_node(pos, node);

        int i = 0;
        while (i < node.n && !(node.keys[i] == kv)) {
            i++;
        }

        if (i < node.n && node.keys[i] == kv) {
            for (int j = i; j < node.n - 1; j++) {
                node.keys[j] = node.keys[j + 1];
            }
            node.n--;
            write_node(pos, node);
        }
    }

public:
    BPlusTree(const string& fname) : filename(fname), root_pos(0), node_count(0) {
        init_file();
    }

    ~BPlusTree() {
        if (file.is_open()) {
            write_header();
            file.close();
        }
    }

    void insert(const string& key_str, int value) {
        KeyValue kv;
        kv.key = Key(key_str);
        kv.value = value;

        // Check if already exists (no duplicate key-value pairs)
        vector<int> existing = find(key_str);
        for (int v : existing) {
            if (v == value) return; // Already exists, don't insert
        }

        Node root;
        read_node(root_pos, root);

        if (root.n == ORDER) {
            Node new_root;
            new_root.is_leaf = false;
            new_root.n = 0;
            new_root.children[0] = root_pos;

            int old_root = root_pos;
            root_pos = allocate_node();
            write_node(root_pos, new_root);

            split_child(new_root, 0, old_root);
            write_header();
        }

        insert_non_full(root_pos, kv);
    }

    vector<int> find(const string& key_str) {
        Key search_key(key_str);
        vector<int> result;

        int pos = find_leaf(search_key);
        Node node;

        while (pos != -1) {
            read_node(pos, node);
            bool found_any = false;

            for (int i = 0; i < node.n; i++) {
                if (node.keys[i].key == search_key) {
                    result.push_back(node.keys[i].value);
                    found_any = true;
                } else if (search_key < node.keys[i].key) {
                    sort(result.begin(), result.end());
                    return result;
                }
            }

            if (!found_any && node.n > 0 && search_key < node.keys[0].key) {
                break;
            }

            pos = node.next;
        }

        sort(result.begin(), result.end());
        return result;
    }

    void remove(const string& key_str, int value) {
        KeyValue kv;
        kv.key = Key(key_str);
        kv.value = value;

        int pos = find_leaf(kv.key);
        remove_from_leaf(pos, kv);
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;
    cin >> n;

    BPlusTree tree("bptree.db");

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string key;
            int value;
            cin >> key >> value;
            tree.insert(key, value);
        } else if (cmd == "delete") {
            string key;
            int value;
            cin >> key >> value;
            tree.remove(key, value);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            vector<int> result = tree.find(key);

            if (result.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < result.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << result[j];
                }
                cout << "\n";
            }
        }
    }

    return 0;
}
