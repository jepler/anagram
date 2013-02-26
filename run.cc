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
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <inttypes.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
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
    char w[1];

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
    worddata *w;
private:
    wordholder &operator=(const wordholder &o);
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

template<class T>
void bwrite(ostream &o, const T &t) {
    o.write(reinterpret_cast<const char *>(&t), sizeof(t));
}

template<class T>
void bwrite(ostream &o, const T *t, size_t n) {
    o.write(reinterpret_cast<const char *>(t), sizeof(T)*n);
}

template<class T>
void bread(istream &o, T &t) {
    o.read(reinterpret_cast<char *>(&t), sizeof(t));
}

template<class T>
void bread(istream &o, T *t, size_t n) {
    o.read(reinterpret_cast<char *>(t), sizeof(T)*n);
}

inline bool ascii(const string &s)
{
    for(string::size_type i=0; i != s.size(); i++)
        if(!isascii(s[i])) return false;
    return true;
}

struct dict {
    struct byinvwordlen {
        byinvwordlen(const dict &d) : d(d) {}
        bool operator()(size_t a, size_t b) const {
            return lcnt(*reinterpret_cast<const worddata*>(&d.wdata[a]))
                > lcnt(*reinterpret_cast<const worddata*>(&d.wdata[b]));
        }
        const dict &d;
    };

    static const int32_t signature = 0x414e4144;
    static const int32_t signature2 = sizeof(size_t);
    static const int32_t signature_rev = 0x44414e41;
    vector<size_t> woff;
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

    void serialize(const char *ofn) const {
        ofstream o(ofn, ios::binary);
        int32_t s = signature;
        bwrite(o, s);
        s = signature2;
        bwrite(o, s);
        bwrite(o, woff.size());
        bwrite(o, &woff[0], woff.size());
        bwrite(o, wdata.size());
        bwrite(o, &wdata[0], wdata.size());
    }

    void deserialize(const char *ifn) {
        ifstream i(ifn, ios::binary);
        int32_t sig;
        bread(i, sig);
        if(sig == signature_rev)
            throw runtime_error("archive is for other-endian machine");
        else if(sig != signature)
            throw runtime_error("not an anagram dictionary archive");

        bread(i, sig);
        if(sig != signature2)
            throw runtime_error("archive is different-size_t machine");

        size_t sz;
        bread(i, sz);
        woff.resize(sz);
        bread(i, &*(woff.begin()), sz);

        bread(i, sz);
        wdata.resize(sz);
        bread(i, &*(wdata.begin()), sz);
    }
};

void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-d dictionary] [-l len1,...] [-m minlen]\n\t"
                            "[-M maxlen] [-a] terms... -- required...\n", progname);
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

size_t total_matches, max_matches;
size_t total_searches, max_searches = ~(size_t)0;

void recurse(ostream &o, worddata &left,
        vector<worddata *>::const_iterator start, vector<worddata *>::const_iterator end,
        vector<worddata *> &stack,
        vector<size_t>::iterator lstart, vector<size_t>::iterator lend,
        vector< vector<worddata *> >::iterator state,
        vector< vector<worddata *> >::iterator stend)
{
    if(total_matches == max_matches) return;
    if(total_searches == max_searches) return;
    if(++total_searches == max_searches) {
        o << "# Reached maximum permitted searches\n";
        return;
    }
    if(!left)
    {
        if(!stack.empty())
        {
            total_matches ++;
            print_stack(o, stack);
            if(total_matches == max_matches) 
                o << "# Reached maximum permitted matches\n";
        }
        return;
    }
    vector<worddata *> &newcandidates = *state;
    newcandidates.clear();
    copy_if(start, end, back_inserter(newcandidates),
            filterer(left));
    for(vector<worddata *>::const_iterator it = newcandidates.begin();
            it != newcandidates.end(); it++)
    {
        stack.push_back(*it);
        if(lstart == lend)
        {
            worddata newleft = left - **it;
            recurse(o, newleft, it, newcandidates.end(), stack, lstart, lend, state+1, stend);
        }
        else if(lcnt(**it) == *lstart) 
        {
            worddata newleft = left - **it;
            recurse(o, newleft, newcandidates.begin(), newcandidates.end(), stack, lstart+1, lend, state+1, stend);
        }
        stack.pop_back();
    }
}

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
    struct rusage u;
    getrusage(RUSAGE_SELF, &u);
    return (u.ru_utime.tv_sec + u.ru_utime.tv_usec * 1e-6)
        + (u.ru_stime.tv_sec + u.ru_stime.tv_usec * 1e-6);
}


int run(dict &d, ostream &o, bool apos, size_t minlen, size_t maxlen, size_t maxcount, vector<size_t> &lengths, string &aw, string &rw) {
    static size_t maxsearch = 100000;
    double t0 = cputime();
    total_matches = 0;
    max_matches = maxcount;
    total_searches = 0;
    max_searches = maxsearch;
    if(!lengths.empty())
    {
        minlen = min(minlen, *min_element(lengths.begin(), lengths.end()));
        maxlen = max(maxlen, *max_element(lengths.begin(), lengths.end()));
    }

    vector<worddata*> pwords;
    pwords.reserve(d.nwords());
    for(size_t i=0; i != d.nwords(); i++)
    {
        worddata *it = d.getword(i);
        if(lcnt(*it) < minlen || lcnt(*it) > maxlen) continue;
        if(!apos && strchr(it->w, '\'')) continue;
        pwords.push_back(it);
    }

    wordholder ww(aw);

    if(lcnt(ww.value()) == 0) return 0;

    vector<worddata *> stack;
    stack.reserve(lcnt(ww.value()));

    wordholder reqd(rw);
    if(!rw.empty())
    {
        ww.value() = ww.value() - reqd.value();
        stack.push_back(reqd.w);
    }

    vector< vector<worddata *> > st;
    st.resize(lcnt(ww.value()));

    recurse(o, ww.value(), pwords.begin(), pwords.end(), stack, lengths.begin(), lengths.end(), st.begin(), st.end());

    o << "# " << total_matches << " matches in " << setprecision(2) << (cputime() - t0) << "s\n";

    return 0;
}

void serve(istream &i, ostream &o, dict &d, bool def_apos, size_t def_minlen, size_t def_maxlen, size_t def_maxcount) {
    string an;
    string reqd;
    
    bool apos = def_apos;
    size_t minlen = def_minlen;
    size_t maxlen = def_maxlen;
    size_t maxcount = def_maxcount;
    vector<size_t> lengths;
    string aw, rw;

    int c;
    while((c = i.peek()) != EOF)
    {
        switch(c) {
            case '\n':
                (void) i.get();
                run(d, o, apos, minlen, maxlen, maxcount, lengths, aw, rw);
                apos = def_apos;
                minlen = def_minlen;
                maxlen = def_maxlen;
                maxcount = def_maxcount;
                lengths.clear();
                aw.clear();
                rw.clear();
                o.put('\n'); o.flush();
                break;
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
}

#ifdef ANA_AS_PYMODULE
PyObject *slice_and_dice(std::string s)
{
    static PyObject *nl = PyString_FromString("\n");
    static PyObject *rstrip = PyString_FromString("rstrip");
    static PyObject *split = PyString_FromString("split");

    PyObject *os = PyString_FromStringAndSize(s.data(), s.size());
    if(!os) return NULL;

    PyObject *ss = PyObject_CallMethodObjArgs(os, rstrip, nl, NULL);
    Py_XDECREF(os);
    if(!ss) return NULL;

    PyObject *xs = PyObject_CallMethodObjArgs(ss, split, nl, NULL);
    Py_XDECREF(ss);
    return xs;
}

struct dict_object {
    PyObject_HEAD
    dict d;
};

PyObject *py_run(PyObject *self, PyObject *args, PyObject *keywds) {
    dict_object *d = (dict_object*)self;
    int apos = false;
    Py_ssize_t minlen = 3;
    Py_ssize_t maxlen = 11;
    Py_ssize_t maxcount = 1000;

    char *terms;
    Py_ssize_t terms_sz;
    static const char * kwlist[]
	= {"terms", "apos", "minlen", "maxlen", "maxcount", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, keywds, "s#|innn:ana.dict.run",
	    const_cast<char**>(kwlist),
	    &terms, &terms_sz, &minlen, &maxlen, &maxcount))
	return NULL;
    
    std::string query(terms, terms+terms_sz);
    std::replace(query.begin(), query.end(), '\n', ' ');
    query += '\n';
    std::istringstream is(query);
    std::ostringstream os;

    serve(is, os, d->d, apos, minlen, maxlen, maxcount);

    return slice_and_dice(os.str());
}

static PyMethodDef dict_methods[] = {
    {"run", reinterpret_cast<PyCFunction>(py_run), METH_VARARGS|METH_KEYWORDS,
	"Run one anagram"},
    {},
};

static PyTypeObject dict_type = {
    PyObject_HEAD_INIT(NULL)
    0,
    "ana.anadict",
    sizeof(dict_object),
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
    self->ob_type->tp_free((PyObject*)self);
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

PyMODINIT_FUNC
initana(void) {
    PyObject *m;
    m = Py_InitModule("ana", methods);

    dict_type.tp_flags = Py_TPFLAGS_DEFAULT;
    dict_type.tp_new = dict_new;
    dict_type.tp_dealloc = reinterpret_cast<destructor>(dict_dealloc);
    dict_type.tp_methods = dict_methods;
    if(PyType_Ready(&dict_type) < 0) return;

    PyModule_AddObject(m, "anadict", (PyObject *)&dict_type);
}

#else
int main(int argc, char **argv)
{
    const char *dictpath=0, *defdict="/usr/share/dict/words", *bindict=0;
    int opt;
    size_t minlen=3, maxlen=11, maxcount=1000;
    bool apos=false, server=false;
    vector<size_t> lengths;
    string aw;

    while((opt = getopt(argc, argv, "-aD:d:M:m:l:s")) != -1)
    {
        switch(opt)
        {
            case 'a': apos = !apos; break;
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
    } else if(bindict) {
        d.deserialize(bindict);
        cout << "# Deserialized " << d.nwords() << " candidate words in " << setprecision(2) << cputime() << "s\n";
    } else {
        d.readdict(dictpath ? dictpath : defdict);
        cout << "# Read " << d.nwords() << " candidate words in " << setprecision(2) << cputime() << "s\n";
    }

    if(server) {
        serve(cin, cout, d, apos, minlen, maxlen, maxcount);
        return 0;
    } else {
        string rw;
        for(int i=optind; i<argc; i++)
        {
            if(rw.empty()) rw = i;
            else rw = rw + " " + argv[i];
        }
        return run(d, cout, apos, minlen, maxlen, maxcount, lengths, aw, rw);
    }
}
#endif
