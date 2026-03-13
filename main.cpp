#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int MAX_KEY_SIZE = 65;
const int M = 85; // B+ tree order - smaller for better balance

struct Key {
    char str[MAX_KEY_SIZE];

    Key() { memset(str, 0, sizeof(str)); }

    Key(const string& s) {
        memset(str, 0, sizeof(str));
        strncpy(str, s.c_str(), MAX_KEY_SIZE - 1);
    }

    int cmp(const Key& other) const {
        return strcmp(str, other.str);
    }

    bool operator<(const Key& other) const {
        return cmp(other) < 0;
    }

    bool operator==(const Key& other) const {
        return cmp(other) == 0;
    }

    bool operator<=(const Key& other) const {
        return cmp(other) <= 0;
    }
};

struct KV {
    Key key;
    int val;

    bool operator<(const KV& o) const {
        int c = key.cmp(o.key);
        return c < 0 || (c == 0 && val < o.val);
    }

    bool operator==(const KV& o) const {
        return key == o.key && val == o.val;
    }
};

struct Node {
    bool leaf;
    int n;
    KV data[M];
    int child[M + 1];
    int next;

    Node() : leaf(true), n(0), next(-1) {
        fill(child, child + M + 1, -1);
    }
};

class BPT {
private:
    fstream f;
    string fname;
    int root;
    int cnt;

    void init() {
        f.open(fname, ios::in | ios::out | ios::binary);
        if (!f || f.peek() == EOF) {
            if (f.is_open()) f.close();
            ofstream tmp(fname, ios::binary);
            tmp.close();
            f.open(fname, ios::in | ios::out | ios::binary);

            root = 0;
            cnt = 1;
            Node r;
            wr(0, r);
            wr_hdr();
        } else {
            rd_hdr();
        }
    }

    void rd_hdr() {
        f.seekg(0);
        f.read((char*)&root, sizeof(root));
        f.read((char*)&cnt, sizeof(cnt));
    }

    void wr_hdr() {
        f.seekp(0);
        f.write((char*)&root, sizeof(root));
        f.write((char*)&cnt, sizeof(cnt));
        f.flush();
    }

    int off(int p) {
        return 2 * sizeof(int) + p * sizeof(Node);
    }

    void rd(int p, Node& nd) {
        f.seekg(off(p));
        f.read((char*)&nd, sizeof(Node));
    }

    void wr(int p, const Node& nd) {
        f.seekp(off(p));
        f.write((char*)&nd, sizeof(Node));
        f.flush();
    }

    int alloc() {
        return cnt++;
    }

    void split(Node& par, int i, int cp) {
        Node c;
        rd(cp, c);

        Node nn;
        nn.leaf = c.leaf;

        int m = M / 2;
        nn.n = c.n - m;

        for (int j = 0; j < nn.n; j++) {
            nn.data[j] = c.data[m + j];
        }

        if (c.leaf) {
            nn.next = c.next;
            c.next = cnt;
            c.n = m;
        } else {
            for (int j = 0; j <= nn.n; j++) {
                nn.child[j] = c.child[m + j];
            }
            c.n = m;
        }

        int np = alloc();
        wr(np, nn);
        wr(cp, c);

        for (int j = par.n; j > i; j--) {
            par.child[j + 1] = par.child[j];
            par.data[j] = par.data[j - 1];
        }

        par.child[i + 1] = np;
        par.data[i] = nn.data[0];
        par.n++;
    }

    bool ins_nf(int p, const KV& kv) {
        Node nd;
        rd(p, nd);

        if (nd.leaf) {
            // Check duplicate
            for (int j = 0; j < nd.n; j++) {
                if (nd.data[j] == kv) return false;
            }

            int i = nd.n - 1;
            while (i >= 0 && kv < nd.data[i]) {
                nd.data[i + 1] = nd.data[i];
                i--;
            }
            nd.data[i + 1] = kv;
            nd.n++;
            wr(p, nd);
            return true;
        } else {
            int i = 0;
            while (i < nd.n && !(kv.key < nd.data[i].key)) {
                i++;
            }

            Node c;
            rd(nd.child[i], c);
            if (c.n == M) {
                split(nd, i, nd.child[i]);
                wr(p, nd);
                rd(p, nd);

                if (!(kv.key < nd.data[i].key)) {
                    i++;
                }
            }
            return ins_nf(nd.child[i], kv);
        }
    }

    int find_lf(const Key& k) {
        int p = root;
        Node nd;
        rd(p, nd);

        while (!nd.leaf) {
            int i = 0;
            while (i < nd.n && !(k < nd.data[i].key)) {
                i++;
            }
            p = nd.child[i];
            rd(p, nd);
        }

        return p;
    }

public:
    BPT(const string& fn) : fname(fn), root(0), cnt(0) {
        init();
    }

    ~BPT() {
        if (f.is_open()) {
            wr_hdr();
            f.close();
        }
    }

    void insert(const string& ks, int v) {
        KV kv;
        kv.key = Key(ks);
        kv.val = v;

        Node r;
        rd(root, r);

        if (r.n == M) {
            Node nr;
            nr.leaf = false;
            nr.n = 0;
            nr.child[0] = root;

            int old_root = root;
            root = alloc();
            wr(root, nr);

            split(nr, 0, old_root);
            wr_hdr();
        }

        ins_nf(root, kv);
    }

    vector<int> find(const string& ks) {
        Key k(ks);
        vector<int> res;

        int p = find_lf(k);
        Node nd;

        while (p != -1) {
            rd(p, nd);
            bool any = false;

            for (int i = 0; i < nd.n; i++) {
                if (nd.data[i].key == k) {
                    res.push_back(nd.data[i].val);
                    any = true;
                } else if (k < nd.data[i].key) {
                    sort(res.begin(), res.end());
                    return res;
                }
            }

            if (!any && nd.n > 0 && k < nd.data[0].key) {
                break;
            }

            p = nd.next;
        }

        sort(res.begin(), res.end());
        return res;
    }

    void del(const string& ks, int v) {
        KV kv;
        kv.key = Key(ks);
        kv.val = v;

        int p = find_lf(kv.key);
        Node nd;
        rd(p, nd);

        for (int i = 0; i < nd.n; i++) {
            if (nd.data[i] == kv) {
                for (int j = i; j < nd.n - 1; j++) {
                    nd.data[j] = nd.data[j + 1];
                }
                nd.n--;
                wr(p, nd);
                return;
            }
        }
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    int n;
    cin >> n;

    BPT tree("bpt.db");

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string key;
            int val;
            cin >> key >> val;
            tree.insert(key, val);
        } else if (cmd == "delete") {
            string key;
            int val;
            cin >> key >> val;
            tree.del(key, val);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            vector<int> res = tree.find(key);

            if (res.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < res.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << res[j];
                }
                cout << "\n";
            }
        }
    }

    return 0;
}
