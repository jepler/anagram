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

#ifndef ANA_H
#define ANA_H

#include <algorithm>
#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <stdexcept>
#include <vector>

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
    wordholder(const std::string &s)
        : w(worddata::make_word(s.c_str())) {}

    ~wordholder() {
        worddata::delete_word(w);
    }
    const worddata &value() const { return *w; }
    worddata &value() { return *w; }
    worddata *w;
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
void bwrite(std::ostream &o, const T &t) {
    o.write(reinterpret_cast<const char *>(&t), sizeof(t));
}

template<class T>
void bwrite(std::ostream &o, const T *t, size_t n) {
    o.write(reinterpret_cast<const char *>(t), sizeof(T)*n);
}

template<class T>
void bread(std::istream &o, T &t) {
    o.read(reinterpret_cast<char *>(&t), sizeof(t));
}

template<class T>
void bread(std::istream &o, T *t, size_t n) {
    o.read(reinterpret_cast<char *>(t), sizeof(T)*n);
}

inline bool ascii(const std::string &s)
{
    for(std::string::size_type i=0; i != s.size(); i++)
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
    std::vector<size_t> woff;
    std::vector<char> wdata;

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
        std::ifstream i(dictpath);
        std::string w;
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
        std::ofstream o(ofn, std::ios::binary);
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
        std::ifstream i(ifn, std::ios::binary);
        int32_t sig;
        bread(i, sig);
        if(sig == signature_rev)
            throw std::runtime_error("archive is for other-endian machine");
        else if(sig != signature)
            throw std::runtime_error("not an anagram dictionary archive");

        bread(i, sig);
        if(sig != signature2)
            throw std::runtime_error("archive is different-size_t machine");

        size_t sz;
        bread(i, sz);
        woff.resize(sz);
        bread(i, &*(woff.begin()), sz);

        bread(i, sz);
        wdata.resize(sz);
        bread(i, &*(wdata.begin()), sz);
    }
};
#endif
