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
#include <unistd.h>
#include "ana.h"

using namespace std;

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
        vector<size_t>::iterator lstart, vector<size_t>::iterator lend,
        vector< std::vector<worddata *> >::iterator state,
        vector< std::vector<worddata *> >::iterator stend)
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
            recurse(newleft, it, newcandidates.end(), stack, lstart, lend, state+1, stend);
        }
        else if(lcnt(**it) == *lstart) 
        {
            worddata newleft = left - **it;
            recurse(newleft, newcandidates.begin(), newcandidates.end(), stack, lstart+1, lend, state+1, stend);
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


int run(dict &d, bool apos, size_t minlen, size_t maxlen, std::vector<size_t> &lengths, std::string &aw, std::string &rw) {
    double t0 = cputime();
    total_matches = 0;

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

    std::vector< std::vector<worddata *> > st;
    st.resize(lcnt(ww.value()));

    recurse(ww.value(), pwords.begin(), pwords.end(), stack, lengths.begin(), lengths.end(), st.begin(), st.end());

    cout << "# " << total_matches << " matches in " << setprecision(2) << (cputime() - t0) << "s\n";

    return 0;
}

void serve(istream &i, dict &d, bool def_apos, size_t def_minlen, size_t def_maxlen) {
    std::string an;
    std::string reqd;
    
    bool apos = def_apos;
    size_t minlen = def_minlen;
    size_t maxlen = def_maxlen;
    std::vector<size_t> lengths;
    std::string aw, rw;

    int c;
    while((c = i.peek()) != EOF)
    {
        switch(c) {
            case '\n':
                (void) i.get();
                run(d, apos, minlen, maxlen, lengths, aw, rw);
                apos = def_apos;
                minlen = def_minlen;
                maxlen = def_maxlen;
                lengths.clear();
                aw.clear();
                rw.clear();
                cout.put('\n'); cout.flush();
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
                    std::string s;
                    (void) i.get();
                    i >> s;
                    if(!rw.empty()) rw = rw + " " + s;
                    else rw = s;
                    break;
                }
            case '?':
                (void) i.get();
                break;
            default: 
                {
                    if(isspace(c)) {
                        (void) i.get();
                        break;
                    }
                    std::string s;
                    i >> s;
                    if(!aw.empty()) aw = aw + " " + s;
                    else aw = s;
                    break;
                }
        }
    }
}

int main(int argc, char **argv)
{
    const char *dictpath=0, *defdict="/usr/share/dict/words", *bindict=0;
    int opt;
    size_t minlen=3, maxlen=11;
    bool apos=false, server=false;
    vector<size_t> lengths;
    string aw;

    while((opt = getopt(argc, argv, "-aD:d:M:m:l:s")) != -1)
    {
        switch(opt)
        {
            case 'a': apos = !apos; break;
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
        serve(cin, d, apos, minlen, maxlen);
        return 0;
    } else {
        std::string rw;
        for(int i=optind; i<argc; i++)
        {
            if(rw.empty()) rw = i;
            else rw = rw + " " + argv[i];
        }
        return run(d, apos, minlen, maxlen, lengths, aw, rw);
    }
}
