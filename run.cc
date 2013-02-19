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

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdint.h>
#include <string>
#include <vector>

using namespace std;

struct worddata
{
    worddata()
    {
        m = 0;
        l = 0;
        w = 0;
        memset(&c, 0, sizeof(c));
    }

    worddata(const char *word)
    {
        m = 0;
        l = 0;
        memset(&c, 0, sizeof(c));

        w = word;
        const char *s = word;
        for(;*s;s++) {
            if(!isalpha(*s)) continue;
            int o = tolower(*s)-'a';
            l++;
            c[o]++;
            m |= (1<<o);
        }
    }

    operator bool() const { return m; }

    unsigned char c[26];
    uint16_t l;
    uint32_t m;
    const char *w;
};

size_t lcnt(const struct worddata &w)
{
    return w.l;
}

bool candidate(const worddata &a, const worddata &b)
{
    if((a.m & b.m) != b.m) return false;
    for(int i=0; i<26; i++)
        if(a.c[i] < b.c[i]) return false;
    return true;
}

worddata operator-(const worddata &a, const worddata &b)
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
    return r;
}

void usage(const char *progname)
{
    fprintf(stderr, "Usage: %s [-d dictionary] [-l len1,...] [-m minlen]\n\t"
                            "[-M maxlen] [-a] terms... -- required...\n", progname);
    exit(1);
}

void print_stack(vector<worddata *> &s)
{
    for(vector<worddata *>::const_iterator it = s.begin(); it != s.end(); it++)
    {
        cout << (*it)->w;
        if(it + 1 == s.end()) cout << "\n";
        else cout << " ";
    }
}

struct filterer
{
    filterer(const worddata &a) : a(a) { }
    bool operator()(worddata *b) { return candidate(a, *b); }
    const worddata &a;
};

size_t total_matches;

void recurse(worddata &left,
        vector<worddata *>::const_iterator start, vector<worddata *>::const_iterator end,
        vector<worddata *> &stack,
        vector<size_t>::iterator lstart, vector<size_t>::iterator lend)
{
    if(!left)
    {
        if(!stack.empty())
        {
            total_matches ++;
            print_stack(stack);
        }
        return;
    }
    vector<worddata *> newcandidates;
    copy_if(start, end, back_inserter(newcandidates),
            filterer(left));
    for(vector<worddata *>::const_iterator it = newcandidates.begin();
            it != newcandidates.end(); it++)
    {
        stack.push_back(*it);
        if(lstart == lend)
        {
            worddata newleft = left - **it;
            recurse(newleft, it, newcandidates.end(), stack, lstart, lend);
        }
        else if(lcnt(**it) == *lstart) 
        {
            worddata newleft = left - **it;
            recurse(newleft, newcandidates.begin(), newcandidates.end(), stack, lstart+1, lend);
        }
        stack.pop_back();
    }
}

bool ascii(const string &s)
{
    for(string::size_type i=0; i != s.size(); i++)
        if(!isascii(s[i])) return false;
    return true;
}

bool byinvwordlen(const worddata &a, const worddata &b)
{
    return lcnt(a) > lcnt(b);
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

int main(int argc, char **argv)
{
    const char *dict = "/usr/share/dict/words";
    int opt;
    size_t minlen=3, maxlen=11;
    bool apos=false;
    vector<size_t> lengths;
    string aw;

    while((opt = getopt(argc, argv, "-ad:M:m:l:")) != -1)
    {
        switch(opt)
        {
            case 'a': apos = !apos; break;
            case 'd': dict = optarg; break;
            case 'M': maxlen = atoi(optarg); break;
            case 'm': minlen = atoi(optarg); break;
            case 'l': lengths = parse_lengths(optarg); break;
            case 1: aw += optarg; break;
            default:  usage(argv[0]);
        }
    }

    if(!lengths.empty())
    {
        minlen = min(minlen, *min_element(lengths.begin(), lengths.end()));
        maxlen = max(maxlen, *max_element(lengths.begin(), lengths.end()));
    }

    ifstream d(dict);
    string w;
    vector<string> words;
    while((d >> w))
    {
        if(!ascii(w)) continue;
        if(!apos && w.find("'") != string::npos) continue;
        // worddata ww(w.c_str());
        // if(lcnt(ww) < minlen || lcnt(ww) > maxlen) continue;
        words.push_back(w);
    }
    vector<worddata> wdata;
    wdata.reserve(words.size());
    vector<worddata*> pwords;
    pwords.reserve(words.size());
    for(vector<string>::iterator it = words.begin(); it != words.end(); it++)
    {
        wdata.push_back(worddata(it->c_str()));
    }
    stable_sort(wdata.begin(), wdata.end(), byinvwordlen);
    for(vector<worddata>::iterator it = wdata.begin(); it != wdata.end(); it++)
    {
        if(lcnt(*it) < minlen || lcnt(*it) > maxlen) continue;
        pwords.push_back(&*it);
    }

    cerr << "# Read " << pwords.size() << " candidate words in " << setprecision(2) << cputime() << "s\n";

    worddata ww(aw.c_str());

    vector<worddata *> stack;
    vector<worddata> reqd;
    for(int i=optind; i < argc; i++)
    {
        worddata rw(argv[i]);
        if(!candidate(ww, rw))
        {
            cerr << "# Cannot make required word " << argv[i] << "\n";
            abort();
        }
        ww = ww - rw;
        reqd.push_back(rw);
    }

    for(vector<worddata>::iterator it = reqd.begin(); it != reqd.end(); it++)
        stack.push_back(&*it);

    recurse(ww, pwords.begin(), pwords.end(), stack, lengths.begin(), lengths.end());

    cerr << "# " << total_matches << " matches in " << setprecision(2) << cputime() << "s\n";

    return 0;
}
