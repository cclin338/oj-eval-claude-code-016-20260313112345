#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int MAX_KEY_SIZE = 65;
const int M = 100; // B+ tree order

struct Key {
    char str[MAX_KEY_SIZE];

    Key() { memset(str, 0, sizeof(str)); }

    Key(const string& s) {
        memset(str, 0, sizeof(str));
        strncpy(str, s.c_str(), MAX_KEY_SIZE - 1);
    }

    bool operator<(const Key& other) const {
        return strcmp(str, other.str) < 0;
    }

    bool operator==(const Key& other) const {
        return strcmp(str, other.str) == 0;
    }

    bool operator<=(const Key& other) const {
        return strcmp(str, other.str) <= 0;
    }
};

struct KeyValue {
    Key key;
    int value;

    bool operator<(const KeyValue& other) const {
        if (key == other.key) return value < other.value;
        return key < other.key;
    }

    bool operator==(const KeyValue& other) const {
        return key == other.key && value == other.value;
    }
};

class BPlusTree {
private:
    struct Node {
        bool is_leaf;
        int n; // number of keys
        KeyValue keys[M];
        int children[M + 1]; // file positions for internal nodes
        int next; // next leaf node (for leaf nodes)

        Node() : is_leaf(true), n(0), next(-1) {
            memset(children, -1, sizeof(children));
        }
    };

    fstream file;
    string filename;
    int root_pos;
    int node_count;

    void init_file() {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file.is_open()) {
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);

            root_pos = 0;
            node_count = 1;
            Node root;
            write_node(root_pos, root);
            write_header();
        } else {
            read_header();
        }
    }

    void read_header() {
        file.seekg(0, ios::beg);
        if (file.peek() == EOF) {
            root_pos = 0;
            node_count = 1;
            Node root;
            write_node(root_pos, root);
            write_header();
        } else {
            file.read((char*)&root_pos, sizeof(root_pos));
            file.read((char*)&node_count, sizeof(node_count));
        }
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

    void split_child(Node& parent, int i, int child_pos) {
        Node child;
        read_node(child_pos, child);

        Node new_node;
        new_node.is_leaf = child.is_leaf;

        int mid = M / 2;
        new_node.n = child.n - mid;

        for (int j = 0; j < new_node.n; j++) {
            new_node.keys[j] = child.keys[mid + j];
        }

        if (!child.is_leaf) {
            for (int j = 0; j <= new_node.n; j++) {
                new_node.children[j] = child.children[mid + j];
            }
        } else {
            new_node.next = child.next;
            child.next = node_count;
        }

        child.n = mid;

        int new_pos = allocate_node();
        write_node(new_pos, new_node);
        write_node(child_pos, child);

        for (int j = parent.n; j > i; j--) {
            parent.children[j + 1] = parent.children[j];
        }
        parent.children[i + 1] = new_pos;

        for (int j = parent.n - 1; j >= i; j--) {
            parent.keys[j + 1] = parent.keys[j];
        }
        parent.keys[i] = new_node.keys[0];
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
            if (child.n == M) {
                split_child(node, i, node.children[i]);
                write_node(pos, node);
                read_node(pos, node);

                if (node.keys[i] < kv || node.keys[i] == kv) {
                    i++;
                }
            }
            insert_non_full(node.children[i], kv);
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

        Node root;
        read_node(root_pos, root);

        if (root.n == M) {
            Node new_root;
            new_root.is_leaf = false;
            new_root.n = 0;
            new_root.children[0] = root_pos;

            int old_root_pos = root_pos;
            root_pos = allocate_node();
            write_node(root_pos, new_root);

            split_child(new_root, 0, old_root_pos);
            write_header();
            insert_non_full(root_pos, kv);
        } else {
            insert_non_full(root_pos, kv);
        }
    }

    vector<int> find(const string& key_str) {
        Key key(key_str);
        vector<int> result;

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

        // Now we're at a leaf, search for all matching keys
        while (pos != -1) {
            read_node(pos, node);
            for (int i = 0; i < node.n; i++) {
                if (node.keys[i].key == key) {
                    result.push_back(node.keys[i].value);
                } else if (key < node.keys[i].key) {
                    break;
                }
            }
            if (node.n > 0 && key < node.keys[node.n - 1].key) {
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

        remove_helper(root_pos, kv);
    }

private:
    void remove_helper(int pos, const KeyValue& kv) {
        Node node;
        read_node(pos, node);

        if (node.is_leaf) {
            int i = 0;
            while (i < node.n && !(node.keys[i] == kv)) {
                i++;
            }

            if (i < node.n) {
                for (int j = i; j < node.n - 1; j++) {
                    node.keys[j] = node.keys[j + 1];
                }
                node.n--;
                write_node(pos, node);
            }
        } else {
            int i = 0;
            while (i < node.n && kv.key < node.keys[i].key) {
                i++;
            }
            remove_helper(node.children[i], kv);
        }
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
