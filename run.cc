/*
 * Copyright © 2013 Jeff Epler <jepler@unpythonic.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef ANA_AS_PYMODULE
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#endif

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <endian.h>
#include <fstream>
#include <functional>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <sys/time.h>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;

struct worddata
{
    worddata()
    {
        m = 0;
        l = 0;
        memset(&c, 0, sizeof(c));
        w[0] = 0;
    }

    operator bool() const { return m; }

    uint32_t m;
    unsigned char c[26];
    uint16_t l;
    char w[1]; // note: not declared as flexible member for Reasons

private:
    /* Note: Constructor assumes that storage for word has been
       allocated!  Doesn't work with stacked or regular new'd storage.
     */
    worddata(const char *word)
    {
        m = 0;
        l = 0;
        strcpy(w, word);
        memset(&c, 0, sizeof(c));

        const char *s = word;
        for(;*s;s++) {
            if(!isalpha(*s)) continue;
            int o = tolower(*s)-'a';
            l++;
            c[o]++;
            m |= (1<<o);
        }
    }


    static worddata *make_word(const char *word) {
        size_t sz = sizeof(worddata) + strlen(word);
        char *storage = new char[sz];
        worddata *r = reinterpret_cast<worddata *>(storage);
        new(r) worddata(word);
        return r;
    }

    static void delete_word(worddata *word) {
        delete[] reinterpret_cast<char*>(word);
    }

    friend struct wordholder;
    friend struct dict;
};

struct wordholder
{
    wordholder() : w(worddata::make_word("")) {}
    wordholder(const wordholder &h) : w(worddata::make_word(h.w->w)) {}
    wordholder(const char *s) : w(worddata::make_word(s)) {}
    wordholder(const string &s)
        : w(worddata::make_word(s.c_str())) {}

    ~wordholder() {
        worddata::delete_word(w);
    }
    const worddata &value() const { return *w; }
    worddata &value() { return *w; }
private:
    worddata *w;
    wordholder &operator=(const wordholder &o) = delete;
};

inline size_t lcnt(const struct worddata &w)
{
    return w.l;
}

inline bool candidate(const worddata &a, const worddata &b)
{
    if((a.m & b.m) != b.m) return false;
    for(int i=0; i<26; i++)
        if(a.c[i] < b.c[i]) return false;
    return true;
}

inline worddata operator-(const worddata &a, const worddata &b)
{
    worddata r;

    for(int i=0; i<26; i++)
    {
        unsigned char tmp;
        r.c[i] = tmp = a.c[i] - b.c[i];
        if(tmp) {
            r.m |= (1<<i);
        }
    }
    r.l = a.l - b.l;
    return r;
}


#if !ANA_JS
template<class T>
void bwrite(ostream &o, const T &t) {
    o.write(reinterpret_cast<const char *>(&t), sizeof(t));
}

template<class T>
void bwrite(ostream &o, const T *t, size_t n) {
    o.write(reinterpret_cast<const char *>(t), sizeof(T)*n);
}

void bwrite_be32(ostream &o, const uint32_t t) {
    uint32_t t_be32 = htobe32(t);
    bwrite(o, t_be32);
}

template<class T>
void bread(istream &o, T &t) {
    o.read(reinterpret_cast<char *>(&t), sizeof(t));
}

template<class T>
void bread(istream &o, T *t, size_t n) {
    o.read(reinterpret_cast<char *>(t), sizeof(T)*n);
}

void bread_be32(istream &o, uint32_t &t) {
    uint32_t t_be32;
    bread(o, t_be32);
    t = htobe32(t_be32);
}

inline bool ascii(const string &s)
{
    for(string::size_type i=0; i != s.size(); i++)
        if(!isascii(s[i])) return false;
    return true;
}
#endif

struct dict {
    struct byinvwordlen {
        byinvwordlen(const dict &d) : d(d) {}
        bool operator()(size_t a, size_t b) const {
            return lcnt(*reinterpret_cast<const worddata*>(&d.wdata[a]))
                > lcnt(*reinterpret_cast<const worddata*>(&d.wdata[b]));
        }
        const dict &d;
    };

    vector<uint32_t> woff;
    vector<char> wdata;

    const worddata *getword(size_t i) const {
        return reinterpret_cast<const worddata*>(&wdata[woff.at(i)]);
    }

    worddata *getword(size_t i) {
        return reinterpret_cast<worddata*>(&wdata[woff.at(i)]);
    }

    // 16-align words
    static size_t pad(size_t t) { return (t + 15) & ~size_t(15); }

    size_t nwords() const { return woff.size(); }

    void addword(const char *word) {
        size_t i = woff.size();
        size_t sz = pad(sizeof(worddata) + strlen(word));
        size_t off = wdata.size();
        wdata.resize(off+sz);
        woff.push_back(off);
        new(getword(i)) worddata(word);
    }

    void readdict(const char *dictpath) {
        ifstream i(dictpath);
        string w;
        while((i >> w))
        {
            if(!ascii(w)) continue;
            addword(w.c_str());
        }
        sort_me();
    }

    void sort_me() {
        stable_sort(woff.begin(), woff.end(), byinvwordlen(*this));
    }

#if !ANA_JS
    static const uint32_t signature = 0x414e4144;
    static const uint32_t signature2 = sizeof(uint64_t);
    static const uint32_t signature_rev = 0x44414e41;
    void serialize(const char *ofn) const {
        ofstream o(ofn, ios::binary);
        uint32_t s = signature;
        bwrite_be32(o, s);
        s = signature2;
        bwrite_be32(o, s);

        if(woff.size() > INT32_MAX) {
            throw runtime_error("dictionary too big (woff)");
        }
        bwrite_be32(o, woff.size());
        bwrite(o, &woff[0], woff.size());
        if(woff.size() > INT32_MAX) {
            throw runtime_error("dictionary too big (wdata)");
        }
        bwrite_be32(o, wdata.size());
        bwrite(o, &wdata[0], wdata.size());
    }

    void deserialize(const char *ifn) {
        ifstream i(ifn, ios::binary);
        uint32_t sig;
        bread_be32(i, sig);
        if(sig == signature_rev)
            throw runtime_error("endianness error");
        else if(sig != signature)
            throw runtime_error("not an anagram dictionary archive");

        bread_be32(i, sig);
        if(sig != signature2)
            throw runtime_error("count-size error");

        uint32_t sz;
        bread_be32(i, sz);
        woff.resize(sz);
        bread(i, &*(woff.begin()), sz);

        bread_be32(i, sz);
        wdata.resize(sz);
        bread(i, &*(wdata.begin()), sz);
    }
#endif
};

void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-d text-dictionary|-D bin-dictionary]\n\t"
                            "[-l len1,...] [-m minlen] [-M maxlen]\n\t"
                            "[-a] [-c] [-s] terms... -- required...\n",
                            progname);
    exit(1);
}

void print_stack(ostream &o, vector<worddata *> &s)
{
    for(vector<worddata *>::const_iterator it = s.begin(); it != s.end(); it++)
    {
        o << (*it)->w;
        if(it + 1 == s.end()) o << "\n";
        else o << " ";
    }
}

struct filterer
{
    filterer(const worddata &a) : a(a) { }
    bool operator()(const worddata *b) const { return candidate(a, *b); }
    const worddata &a;
};

vector<size_t> parse_lengths(const char *l)
{
    istringstream s(l);
    vector<size_t> r;
    size_t i;
    while((s >> i)) {
        r.push_back(i);
        if(s.peek() == ',') s.get();
    }
    return r;
}

#include <sys/time.h>
#include <sys/resource.h>
double cputime()
{
#if defined(ANA_AS_JS)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
#else
    struct rusage u;
    getrusage(RUSAGE_SELF, &u);
    return (u.ru_utime.tv_sec + u.ru_utime.tv_usec * 1e-6)
        + (u.ru_stime.tv_sec + u.ru_stime.tv_usec * 1e-6);
#endif
}


struct ana_cfg {
    ana_cfg()
    : apos(0), just_candidates(0), minlen(3), maxlen(10),
            total_matches(0), max_matches(1000), total_searches(0),
            max_searches(1000000) {}

    ana_cfg(bool apos, bool just_candidates, size_t minlen, size_t maxlen,
            size_t max_matches,
            size_t max_searches, const vector<size_t>& lengths,
            const string &ws, const string &rs)
    : apos(apos), just_candidates(just_candidates), minlen(minlen),
            maxlen(maxlen),
            total_matches(0), max_matches(max_matches), total_searches(0),
            max_searches(max_searches), lengths(lengths), rs(rs), ww(wordholder(ws).value() - wordholder(rs).value()) {
    }

    bool apos, just_candidates;
    size_t minlen, maxlen;
    size_t total_matches, max_matches, total_searches, max_searches;
    vector<size_t> lengths;
    std::string rs;
    worddata ww;
};

struct ana_frame {
    worddata l;
    vector<const worddata*> c;
    vector<const worddata*>::iterator st, en;
    vector<size_t>::iterator lst, len;
};

struct ana_st {
    ana_cfg cfg;
    double t0;
    vector<ana_frame> fr;
    vector<const char *> words;
};

void setup(ana_st &st, const ana_cfg &cfg, const dict &d)
{
    st.cfg = cfg;
    st.t0 = cputime();
    st.fr.clear();
    st.fr.reserve(lcnt(cfg.ww));
    st.fr.push_back(ana_frame());
    ana_frame &f = st.fr.back();
    f.l = st.cfg.ww;
    f.lst = st.cfg.lengths.begin();
    f.len = st.cfg.lengths.end();
    filterer fi(f.l);
    for(size_t i=0; i != d.nwords(); i++)
    {
        const worddata *it = d.getword(i);
        if(lcnt(*it) < cfg.minlen || lcnt(*it) > cfg.maxlen) continue;
        if(!cfg.apos && strchr(it->w, '\'')) continue;
        if(!fi(it)) continue;
        f.c.push_back(it);
    }
    f.st = f.c.begin();
    f.en = f.c.end();
    st.words.clear();
    if(!st.cfg.rs.empty()) st.words.push_back(st.cfg.rs.c_str());
}

bool words_to_string(const vector<const char*> words, std::string &resultline) {
    resultline.clear();

    for(vector<const char *>::const_iterator it = words.begin(); it != words.end(); it++)
    {
        if(!resultline.empty()) resultline += ' ';
        resultline += *it;
    }
    return true;
}

bool step(ana_st &st, string &resultline) {
    resultline = string();

    if(st.cfg.total_matches == st.cfg.max_matches) {
        ostringstream o;
        o << "# Reached maximum of " << st.cfg.total_matches << " matches in " << setprecision(2) << (cputime() - st.t0) << "s";
        resultline = o.str();
        return false;
    }

    while(!st.fr.empty()) {
        if(st.cfg.total_searches == st.cfg.max_searches) {
            ostringstream o;
            o << "# Reached maximum of " << st.cfg.total_searches << " searches in " << setprecision(2) << (cputime() - st.t0) << "s";
            resultline = o.str();
            return false;
        }

        st.cfg.total_searches ++;

        ana_frame &f = st.fr.back();

        if(!f.l) {
            st.cfg.total_matches ++;
            words_to_string(st.words, resultline);
            st.words.pop_back();
            st.fr.pop_back();
            return true;
        }

        if(f.lst != f.len) {
            size_t reqlen = *f.lst;
            while(f.st != f.en && lcnt(**f.st) != reqlen) f.st ++;
        }

        if(f.st == f.en)
        {
            st.fr.pop_back();
            st.words.pop_back();
            continue;
        }

        if(st.cfg.just_candidates) {
            st.cfg.total_matches ++;
            resultline = std::string((*f.st)->w);
            f.st++;
            return true;
        }

        // guaranteed not to move, because of earlier reserve()!
        assert(st.fr.size() + 1 < st.fr.capacity());
        st.fr.push_back(ana_frame());

        st.words.push_back((*f.st)->w);

        ana_frame &nf = st.fr.back();
        nf.l = f.l - **f.st;
        nf.c.clear();
        if(f.lst != f.len) {
            copy_if(f.c.begin(), f.en, back_inserter(nf.c), filterer(nf.l));
            nf.lst = f.lst + 1;
            nf.len = f.len;
        } else {
            copy_if(f.st, f.en, back_inserter(nf.c), filterer(nf.l));
            nf.lst = nf.len = f.len;
        }
        nf.st = nf.c.begin();
        nf.en = nf.c.end();
        
        f.st++;
    }

    ostringstream o;
    o << "# " << st.cfg.total_matches << " matches ("
        << st.cfg.total_searches << " searches) in "
        << setprecision(2) << (cputime() - st.t0) << "s";
    resultline = o.str();
    return false;
}

int run(dict &d, ostream &o, ana_cfg &cfg) {
    ana_st st;
    setup(st, cfg, d);
    while(1) {
        std::string line;
        bool res = step(st, line);
        o << line << endl;
        if(!res) break;
    }
    return 0;
}


size_t maxsearch = 10000000;
int run(dict &d, ostream &o, bool apos, bool just_candidates, size_t minlen, size_t maxlen, size_t maxcount, vector<size_t> &lengths, string &aw, string &rw) {
    ana_cfg cfg(apos, just_candidates, minlen, maxlen, maxcount, maxsearch,
        lengths, aw, rw);
    return run(d, o, cfg);
}

void parse(const std::string &s, ana_cfg &st, bool def_apos, bool def_just_candidates, size_t def_minlen, size_t def_maxlen, size_t def_maxcount) {
    string an;
    string reqd;
    
    bool apos = def_apos;
    bool just_candidates = def_just_candidates;
    size_t minlen = def_minlen;
    size_t maxlen = def_maxlen;
    size_t maxcount = def_maxcount;
    vector<size_t> lengths;
    string aw, rw;

    int c;

    istringstream i(s);

    while((c = i.peek()) != EOF)
    {
        switch(c) {
            case '<':
                (void) i.get();
                i >> maxlen; break;
            case '>':
                (void) i.get();
                i >> minlen; break;
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                {
                    size_t len;
                    i >> len;
                    lengths.push_back(len);
                    minlen = std::min(minlen, len);
                    maxlen = std::max(maxlen, len);
                    break;
                }
            case '+': case '=':
                {
                    string s;
                    (void) i.get();
                    i >> s;
                    if(!rw.empty()) rw = rw + " " + s;
                    else rw = s;
                    break;
                }
            case '-':
                {
                    (void) i.get();
                    i >> maxcount;
                    maxcount = min(def_maxcount, maxcount);
                    break;
                }
            case '\'':
                (void) i.get();
                apos = !apos;
                break;
            case '?':
                (void) i.get();
                just_candidates = !just_candidates;
                break;
            default: 
                {
                    if(isspace(c)) {
                        (void) i.get();
                        break;
                    }
                    string s;
                    i >> s;
                    if(!aw.empty()) aw = aw + " " + s;
                    else aw = s;
                    break;
                }
        }
    }
    st.~ana_cfg();
    new(&st) ana_cfg(apos, just_candidates, minlen, maxlen, maxcount, maxsearch,
                    lengths, aw, rw);
}

void serve(istream &i, ostream &o, dict &d, bool def_apos, bool def_just_candidates, size_t def_minlen, size_t def_maxlen, size_t def_maxcount) {
    string s;
    while((getline(i, s))) {
        ana_cfg cfg;
        parse(s, cfg, def_apos, def_just_candidates, def_minlen, def_maxlen, def_maxcount);
        run(d, o, cfg);
        o.put('\n'); o.flush();
    }
}

#if defined(ANA_AS_PYMODULE)
struct dict_object {
    PyObject_HEAD
    dict d;
};

static PyTypeObject dict_type = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "ana.anadict",
    .tp_basicsize = sizeof(dict_object),
};

struct search_object {
    PyObject_HEAD;
    dict_object *d;
    ana_st *st;
};

static PyTypeObject search_type = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "ana.results",
    .tp_basicsize = sizeof(search_object),
};

search_object *search_new(PyTypeObject *type, PyObject *args, PyObject *kw)
{
    return reinterpret_cast<search_object*>(type->tp_alloc(type, 0));
}

PyObject *py_run(PyObject *self, PyObject *args, PyObject *keywds) {
    dict_object *d = (dict_object*)self;
    int apos = false, just_candidates = false;
    Py_ssize_t minlen = 3;
    Py_ssize_t maxlen = 11;
    Py_ssize_t maxcount = 1000;

    char *terms;
    Py_ssize_t terms_sz;
    static const char * kwlist[] = {
        "terms", "apos", "just_candidates", "minlen", "maxlen", "maxcount",
        NULL};
    if(!PyArg_ParseTupleAndKeywords(args, keywds, "s#|iinnn:ana.dict.run",
	    const_cast<char**>(kwlist),
	    &terms, &terms_sz, &apos, &just_candidates,
            &minlen, &maxlen, &maxcount))
	return NULL;
    
    string query(terms, terms+terms_sz);

    search_object *o = search_new(&search_type, 0, 0);
    o->d = d;
    Py_INCREF(d);

    ana_cfg cfg;
    o->st = new ana_st;
    parse(query, cfg, apos, just_candidates, minlen, maxlen, maxcount);
    setup(*o->st, cfg, d->d);

    return reinterpret_cast<PyObject*>(o);
}

static PyMethodDef dict_methods[] = {
    {"run", reinterpret_cast<PyCFunction>(py_run), METH_VARARGS|METH_KEYWORDS,
	"Run one anagram"},
    {},
};
static PyObject *
dict_new(PyTypeObject *type, PyObject *args, PyObject *kw) {
    dict_object *self = reinterpret_cast<dict_object*>(type->tp_alloc(type, 0));
    new(&self->d) dict();
    return (PyObject *)self;
}

static void
dict_dealloc(dict_object *self) {
    self->d.~dict();
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
py_fromascii(PyObject *self, PyObject *args) {
    PyObject *d = dict_new(&dict_type, NULL, NULL);
    dict_object *r = reinterpret_cast<dict_object*>(d);
    char *path;
    if(!PyArg_ParseTuple(args, "s", &path)) return NULL;
    r->d.readdict(path);
    return d;
}

static PyObject *
py_frombin(PyObject *self, PyObject *args) {
    PyObject *d = dict_new(&dict_type, NULL, NULL);
    dict_object *r = reinterpret_cast<dict_object*>(d);
    char *path;
    if(!PyArg_ParseTuple(args, "s", &path)) return NULL;
    r->d.deserialize(path);
    return d;
}

static PyMethodDef methods[] = {
    {"from_ascii", py_fromascii, METH_VARARGS, "Parse ASCII dictionary"},
    {"from_binary", py_frombin, METH_VARARGS, "Parse binary dictionary"},
    {},
};

static PyObject *
search_iter(PyObject *self) {
    Py_INCREF(self);
    return self;
}

// Note: it's up to the user to ensure no more than one thread is calling
// .next() on a specific search_object at the same time
static PyObject *
search_iternext(search_object *self) {
    std::string result;
    if(!self->st) return NULL;
    Py_BEGIN_ALLOW_THREADS
    bool res = step(*self->st, result);
    if(!res) { delete self->st; self->st = NULL; }
    Py_END_ALLOW_THREADS
    return PyUnicode_FromStringAndSize(result.data(), result.size());
}

static void
search_dealloc(search_object *self) {
    Py_XDECREF(self->d);
    delete self->st;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "ana",
    "Quickly compute anagrams of strings",
    -1,
    methods
};

PyMODINIT_FUNC
PyInit_ana(void) {
    PyObject *m;
    m = PyModule_Create(&moduledef);

    dict_type.tp_flags = Py_TPFLAGS_DEFAULT;
    dict_type.tp_new = dict_new;
    dict_type.tp_dealloc = reinterpret_cast<destructor>(dict_dealloc);
    dict_type.tp_methods = dict_methods;
    if(PyType_Ready(&dict_type) < 0) return NULL;

    search_type.tp_flags = Py_TPFLAGS_DEFAULT;
    search_type.tp_new = reinterpret_cast<newfunc>(search_new);
    search_type.tp_dealloc = reinterpret_cast<destructor>(search_dealloc);
    search_type.tp_iter = reinterpret_cast<getiterfunc>(search_iter);
    search_type.tp_iternext =
        reinterpret_cast<iternextfunc>(search_iternext);
    if(PyType_Ready(&search_type) < 0) return NULL;

    PyModule_AddObject(m, "anadict", (PyObject *)&dict_type);

    return m;
}

#elif defined(ANA_AS_JS)
#include <emscripten/bind.h>

dict d;

std::string js_run(std::string s) {
    std::string result;
    ana_cfg cfg;
    ana_st st;
    parse(s, cfg, false, false, 3, 11, 10000);
    if (!d.nwords()) {
        std::cerr << "# reading words\n";
        d.readdict("words");
    }
    setup(st, cfg, d);
    while(1) {
        std::string line;
        bool res = step(st, line);
        result += line; result += "\n";
        if(!res) break;
    }
    return result;
}

EMSCRIPTEN_BINDINGS(ana) {
    emscripten::function("ana", &js_run);
}

#else
int main(int argc, char **argv)
{
    const char *dictpath=0, *defdict="/usr/share/dict/words", *bindict=0;
    int opt;
    size_t minlen=3, maxlen=11, maxcount=1000;
    bool apos=false, just_candidates=false, server=false;
    vector<size_t> lengths;
    string aw;

    while((opt = getopt(argc, argv, "-acD:d:M:m:l:s")) != -1)
    {
        switch(opt)
        {
            case 'a': apos = !apos; break;
            case 'c': just_candidates = !just_candidates; break;
            case 'L': maxcount = atoi(optarg); break;
            case 'D': bindict = optarg; break;
            case 'd': dictpath = optarg; break;
            case 'M': maxlen = atoi(optarg); break;
            case 'm': minlen = atoi(optarg); break;
            case 'l': lengths = parse_lengths(optarg); break;
            case 's': server = !server; break;
            case 1: aw += optarg; break;
            default:  usage(argv[0]);
        }
    }

    dict d;
    if(bindict && dictpath) {
        d.readdict(dictpath);
        d.serialize(bindict);
        cout << "# Read and serialized " << d.nwords() << " candidate words in " << setprecision(2) << cputime() << "s\n";
        return 0;
    } else if(bindict) {
        d.deserialize(bindict);
        cout << "# Deserialized " << d.nwords() << " candidate words in " << setprecision(2) << cputime() << "s\n";
    } else {
        d.readdict(dictpath ? dictpath : defdict);
        cout << "# Read " << d.nwords() << " candidate words in " << setprecision(2) << cputime() << "s\n";
    }

    if(server) {
        serve(cin, cout, d, apos, just_candidates, minlen, maxlen, maxcount);
        return 0;
    } else {
        string rw;
        for(int i=optind; i<argc; i++)
        {
            if(rw.empty()) rw = argv[i];
            else rw = rw + " " + argv[i];
        }
        return run(d, cout, apos, just_candidates, minlen, maxlen, maxcount, lengths, aw, rw);
    }
}
#endif
