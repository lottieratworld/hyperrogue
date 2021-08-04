// Copyright (C) 2018 Zeno and Tehora Rogue, see 'hyper.cpp' for details
// this is a plugin which generates branched tilings for newconformist 
// https://github.com/zenorogue/newconformist (see the option '-cvl')

#include "../hyper.h"

namespace hr {
#if CAP_SHOT

struct location {
  transmatrix lView;
  cell *lco;
  };

struct lineinfo {
  vector<location> locs;
  int plus_matrices;
  int minus_matrices;
  };

map<int, lineinfo> lines;

location loc_multiply(location orig, transmatrix T) {
  dynamicval<transmatrix> dv(View, orig.lView);
  dynamicval<cell*> dc(centerover, orig.lco);
  View = inverse(T) * View;
  for(int a=0; a<10; a++) optimizeview();
  return location{View, centerover};
  }

bool show_map = false;

void cvl_marker() {
  if(show_map) for(auto& l: lines) {
    int id = 0;
    for(auto& loc: l.second.locs) {
      if(gmatrix.count(loc.lco)) {
        shiftmatrix T = gmatrix[loc.lco] * inverse(loc.lView);
        queuepoly(T, cgi.shAsymmetric, 0xFF00FFFF);
        queuestr(T, 1.0, its(l.first)+"/"+its(id), 0xFFFFFF);
        }
      id++;
      }
    }
  }

int readArgs() {
  using namespace arg;
           
  if(0) ;
  else if(argis("-cvlbuild")) {
    PHASEFROM(3);
    start_game();
    shift(); 
    fhstream f(argcs(), "rt");
    if(!f.f) { shift(); printf("failed to open file\n"); return 0; }
    int id;
    lineinfo l0;    
    scan(f, id, l0.plus_matrices, l0.minus_matrices);
    l0.locs.push_back(location{View, centerover});
    for(int i=1; i<l0.plus_matrices; i++)
      l0.locs.push_back(loc_multiply(l0.locs.back(), xpush(1)));
    lines[id] = std::move(l0);
    
    while(true) {
      scan(f, id);
      println(hlog, "id=", id, ".");
      if(id < 0) break;
      auto& l1 = lines[id];
      int step;
      scan(f, id, step);
      transmatrix T;
      for(int a=0; a<9; a++) scan(f, T[0][a]);
      scan(f, l1.plus_matrices, l1.minus_matrices);
      auto old = lines[id].locs[step];
      println(hlog, "FROM ", old.lView, old.lco, " id=", id, " step=", step);
      l1.locs.push_back(loc_multiply(old, T));
      println(hlog, "TO ", l1.locs.back().lView, l1.locs.back().lco, "; creating ", l1.plus_matrices);
      for(int i=1; i<l1.plus_matrices; i++)
        l1.locs.push_back(loc_multiply(l1.locs.back(), xpush(1)));
      println(hlog, "LAST ", l1.locs.back().lView, l1.locs.back().lco);
      }
    }
  else if(argis("-cvllist")) {
    for(auto& l: lines)
      for(auto& loc: l.second.locs) {
        println(hlog, l.first, ". ", loc.lco, " (dist=", celldist(loc.lco), "), View = ", loc.lView);
        }
    }
  else if(argis("-cvlmap")) {
    show_map = !show_map;
    }
  else if(argis("-cvldraw")) {
    shift(); string s = args();
    for(auto& p: lines) {
      int i = 0;
      for(auto& loc: p.second.locs) {
        dynamicval<transmatrix> dv(View, loc.lView);
        dynamicval<cell*> dc(centerover, loc.lco);
        shot::take(format(s.c_str(), p.first, i++));
        }
      }
    }
  else return 1;
  return 0;
  }

auto magichook = addHook(hooks_args, 100, readArgs) + addHook(hooks_frame, 100, cvl_marker);
#endif

 
}
