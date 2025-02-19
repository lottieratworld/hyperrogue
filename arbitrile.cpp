// Hyperbolic Rogue -- Arbitrary Tilings
// Copyright (C) 2011-2019 Zeno Rogue, see 'hyper.cpp' for details

/** \file arbitrile.cpp 
 *  \brief Arbitrary tilings
 *
 *  Arbitrary tilings, defined in .tes files.
 */

#include "hyper.h"
namespace hr {

EX namespace arb {

EX int affine_limit = 200;

EX bool legacy; /* angleofs command */

#if HDR

/** a type used to specify the connections between shapes */
struct connection_t {
  /** the index of the connected shape in the 'shapes' table */
  int sid;
  /** the index of the edge in the 'shapes' table */
  int eid;
  /** 1 if this connection mirrored, 0 otherwise. do_unmirror() removes all mirrors by doubling shapes */
  int mirror;
  };

inline void print(hstream& hs, const connection_t& conn) { print(hs, tie(conn.sid, conn.eid, conn.mirror)); }

/** \brief each shape of the arb tessellation
 *  note: the usual HyperRogue convention is: vertex 0, edge 0, vertex 1, edge 1, ... 
 *  note: the tesfile convention is: edge 0, vertex 0, edge 1, vertex 1, ...
 */

struct shape {
  /** index in the arbi_tiling::shapes */
  int id;
  /** flags such as sfLINE and sfPH */
  int flags;
  /** list of vertices in the usual convention */
  vector<hyperpoint> vertices;
  /** list of vertices in the tesfile convention */
  vector<ld> angles;
  /** list of edge lengths */
  vector<ld> edges;
  /** list of edge connections */
  vector<connection_t> connections;
  int size() const { return isize(vertices); }
  void build_from_angles_edges(bool is_comb);
  vector<pair<int, int> > sublines;
  vector<pair<ld, ld>> stretch_shear;
  int repeat_value;
  /** if a tile/edge combination may be connected to edges j1 and j2 of this, j1-j2 must be divisible by cycle_length */
  int cycle_length;
  /** list of valences of vertices in the tesfile convention */
  vector<int> vertex_valence;
  /** list of periods of vertices in the tesfile convention */
  vector<int> vertex_period;
  /** list of angles at vertices in the tesfile convention */
  vector<vector<ld>> vertex_angles;
  };

struct slider {
  string name;
  ld zero;
  ld current;
  ld min;
  ld max;
  };

struct arbi_tiling {

  int order;
  /* have_line and have_ph: line and ph flags have been marked for tiles */
  bool have_line, have_ph;
  /* is the tree structure given in the tes file */
  bool have_tree;
  /* use "star." if the tessellation includs star polygons */
  bool is_star;
  /* use "combinatorial." for combinatorial tessellations; vertex valences computed based on their angles. Currently only rulegen works for combinatorial tessellations */
  bool is_combinatorial;
  /* reserved for future flags */
  bool res0, res1, res2, res3;

  int yendor_backsteps;

  vector<shape> shapes;
  string name;
  string comment;
  
  vector<slider> sliders;
  
  ld cscale;
  int range;
  ld floor_scale;
  ld boundary_ratio;
  string filename;
  int mirror_rules;
  
  vector<string> options;

  int min_valence, max_valence;

  geometryinfo1& get_geometry();
  eGeometryClass get_class() { return get_geometry().kind; }

  ld scale();
  };
#endif

EX arbi_tiling current;

EX bool using_slided;

EX arbi_tiling slided;

EX bool in_slided() { return in() && using_slided; }

EX arbi_tiling& current_or_slided() {
  return using_slided ? slided : current;
  }

/** id of vertex in the arbitrary tiling */

EX short& id_of(heptagon *h) { return h->zebraval; }

#if HDR
struct hr_polygon_error : hr_exception {
  vector<transmatrix> v;
  eGeometryClass c;
  int id;
  transmatrix end;
  map<string, cld> params;
  hr_polygon_error(const vector<transmatrix>& _v, int _id, transmatrix _e) : v(_v), c(cgclass), id(_id), end(_e) {}
  ~hr_polygon_error() noexcept(true) {}
  string generate_error();
  };
#endif

string hr_polygon_error::generate_error() {
  cld dist = (hdist0(tC0(end)) / params["distunit"]);
  bool angle = abs(dist) < 1e-9;
  if(angle) dist = (atan2(end * xpush0(1)) / params["angleunit"]);     
  return
    XLAT("Polygon number %1 did not close correctly (%2 %3). Here is the picture to help you understand the issue.\n\n", its(id), 
      angle ? "angle" : "distance",
      lalign(0, dist)
      );
  }

struct connection_debug_request : hr_exception {
  int id;
  eGeometryClass c;
  connection_debug_request(int i): id(i), c(cgclass) {}
  };

void ensure_geometry(eGeometryClass c) {
  stop_game();
  if(c != cgclass) {
    if(c == gcEuclid) set_geometry(gEuclid);
    if(c == gcHyperbolic) set_geometry(gNormal);
    if(c == gcSphere) set_geometry(gSphere);
    }

  if(specialland != laCanvas) {   
    canvas_default_wall = waInvisibleFloor;
    patterns::whichCanvas = 'g';
    patterns::canvasback = 0xFFFFFF;
    enable_canvas();
    }
  start_game();
  }

void start_poly_debugger(hr_polygon_error& err) {
  #if CAP_EDIT
  ensure_geometry(err.c);

  drawthemap();

  mapeditor::drawing_tool = true;
  pushScreen(mapeditor::showDrawEditor);
  mapeditor::initdraw(cwt.at);
  
  int n = isize(err.v);
  
  mapeditor::dtcolor = 0xFF0000FF;
  mapeditor::dtwidth = 0.02;
  for(int i=0; i<n-1; i++)
    mapeditor::dt_add_line(shiftless(tC0(err.v[i])), shiftless(tC0(err.v[i+1])), 0);
  
  mapeditor::dtcolor = 0xFFFFFFFF;
  for(int i=0; i<n; i++)
    mapeditor::dt_add_text(shiftless(tC0(err.v[i])), 0.5, its(i));
  #endif
  }

void shape::build_from_angles_edges(bool is_comb) {
  transmatrix at = Id;
  vertices.clear();
  int n = isize(angles);
  hyperpoint ctr = Hypc;
  vector<transmatrix> matrices;
  if(!legacy) for(auto& a: angles) a += M_PI;
  for(int i=0; i<n; i++) {
    matrices.push_back(at);
    if(debugflags & DF_GEOM) println(hlog, "at = ", at);
    vertices.push_back(tC0(at));
    ctr += tC0(at);
    at = at * xpush(edges[i]) * spin(angles[i]);
    }
  matrices.push_back(at);
  if(!legacy) for(auto& a: angles) a -= M_PI;
  if(is_comb) return;
  if(!eqmatrix(at, Id)) {
    throw hr_polygon_error(matrices, id, at);
    }
  if(sqhypot_d(3, ctr) < 1e-2) {
    // this may happen for some spherical tilings
    // try to move towards the center
    if(debugflags & DF_GEOM) println(hlog, "special case encountered");
    for(int i=0; i<n; i++) {
      ctr += at * xpush(edges[i]) * spin((angles[i]+M_PI)/2) * xpush0(.01);
      at = at * xpush(edges[i]) * spin(angles[i]);
      }
    if(debugflags & DF_GEOM) println(hlog, "ctr = ", ctr);
    }
  ctr = normalize(ctr);
  for(auto& v: vertices) v = gpushxto0(ctr) * v;
  }

EX bool correct_index(int index, int size) { return index >= 0 && index < size; }
template<class T> bool correct_index(int index, const T& v) { return correct_index(index, isize(v)); }

template<class T> void verify_index(int index, const T& v, exp_parser& ep) { if(!correct_index(index, v)) throw hr_parse_exception("bad index: " + its(index) + " at " + ep.where()); }

string unnamed = "unnamed";

EX void load_tile(exp_parser& ep, arbi_tiling& c, bool unit) {
  c.shapes.emplace_back();
  auto& cc = c.shapes.back();
  cc.id = isize(c.shapes) - 1;
  cc.flags = 0;
  cc.repeat_value = 1;
  while(ep.next() != ')') {
    cld dist = 1;
    if(!unit) {
      dist = ep.parse(0);
      ep.force_eat(",");
      }
    cld angle = ep.parse(0);
    cc.edges.push_back(ep.validate_real(dist * ep.extra_params["distunit"]));
    cc.angles.push_back(ep.validate_real(angle * ep.extra_params["angleunit"] + ep.extra_params["angleofs"]));
    if(ep.eat(",")) continue;
    else if(ep.eat(")")) break;
    else throw hr_parse_exception("expecting , or )");
    }
  try {
    cc.build_from_angles_edges(c.is_combinatorial);
    }
  catch(hr_parse_exception& ex) {
    throw hr_parse_exception(ex.s + ep.where());
    }
  catch(hr_polygon_error& poly) {
    poly.params = ep.extra_params;
    throw;
    }
  cc.connections.resize(cc.size());
  for(int i=0; i<isize(cc.connections); i++)
    cc.connections[i] = connection_t{cc.id, i, false};
  cc.stretch_shear.resize(cc.size(), make_pair(1, 0));
  }

EX bool do_unmirror = true;

/** \brief for tessellations which contain mirror rules, remove them by taking the orientable double cover */
EX void unmirror() {
  auto& mirror_rules = arb::current.mirror_rules;
  mirror_rules = 0;
  for(auto& s: arb::current.shapes)
    for(auto& t: s.connections)
      if(t.mirror)
        mirror_rules++;
  if(!mirror_rules) return;
  auto& sh = current.shapes;
  int s = isize(sh);
  for(int i=0; i<s; i++)
    sh.push_back(sh[i]);
  for(int i=0; i<2*s; i++)
    sh[i].id = i;
  for(int i=s; i<s+s; i++) {
    for(auto& v: sh[i].vertices) 
      v[1] = -v[1];
    reverse(sh[i].edges.begin(), sh[i].edges.end());
    reverse(sh[i].vertices.begin()+1, sh[i].vertices.end());
    reverse(sh[i].angles.begin(), sh[i].angles.end()-1);
    reverse(sh[i].connections.begin(), sh[i].connections.end());
    }

  if(true) for(int i=0; i<s+s; i++) {
    for(auto& co: sh[i].connections) {
      bool mirr = co.mirror ^ (i >= s);
      co.mirror = false;
      if(mirr) {
        co.sid += s;
        co.eid = isize(sh[co.sid].angles) - 1 - co.eid;
        }
      }
    }
  }

EX void compute_vertex_valence() {
  auto& ac = arb::current;

  int tcl = -1;

  for(auto& sh: ac.shapes)
    sh.cycle_length = isize(sh.vertices);
  
  recompute:
  while(true) {

    for(auto& sh: ac.shapes) {
      int i = sh.id;
      int n = isize(sh.vertices);
      
      for(int k=sh.cycle_length; k<n; k++) {
        auto co = sh.connections[k];
        auto co1 = sh.connections[k-sh.cycle_length];
        if(co.sid != co1.sid) {
          println(hlog, "ik = ", tie(i,k), " co=", co, "co1=", co1, " cl=", sh.cycle_length);
          throw hr_parse_exception("connection error #2 in compute_vertex_valence");
          }
        ac.shapes[co.sid].cycle_length = abs(gcd(ac.shapes[co.sid].cycle_length, co.eid - co1.eid));
        }

      for(int k=0; k<n; k++) {
        auto co = sh.connections[k];
        co = ac.shapes[co.sid].connections[co.eid];
        if(co.sid != i) throw hr_parse_exception("connection error in compute_vertex_valence");
        sh.cycle_length = abs(gcd(sh.cycle_length, k-co.eid));
        }
      if(debugflags & DF_GEOM) 
        println(hlog, "tile ", i, " cycle_length = ", sh.cycle_length, " / ", n);
      }
    
    int new_tcl = 0;
    for(auto& sh: ac.shapes) {
      auto& len = sh.cycle_length;
      if(len < 0) len = -len;
      new_tcl += len;
      }

    if(new_tcl == tcl) break;
    tcl = new_tcl;
    }
  
  if(cgflags & qAFFINE) return;
  if(ac.is_star) return;

  for(auto& sh: ac.shapes) {
    int n = sh.size();
    int i = sh.id;
    sh.vertex_valence.resize(n);
    sh.vertex_period.resize(n);
    sh.vertex_angles.resize(n);
    for(int k=0; k<n; k++) {
      ld total = 0;
      int qty = 0, pqty = 0;
      connection_t at = {i, k, false};
      connection_t at1 = at;
      vector<ld> anglelist;
      do {
        if(at.sid == at1.sid && (at.eid-at1.eid) % ac.shapes[at.sid].cycle_length == 0) pqty = 0;
        ld a = ac.shapes[at.sid].angles[at.eid];
        while(a < 0) a += 360 * degree;
        while(a > 360 * degree) a -= 360 * degree;
        total += a;
        anglelist.push_back(a);
        qty++;
        pqty++;

        at.eid++;
        if(at.eid == isize(ac.shapes[at.sid].angles)) at.eid = 0;

        at = ac.shapes[at.sid].connections[at.eid];
        }
      while(total < 360*degree - 1e-6);
      if(total > 360*degree + 1e-6) throw hr_parse_exception("improper total in compute_stats");
      if(at.sid != i) throw hr_parse_exception("ended at wrong type determining vertex_valence");
      if((at.eid - k) % ac.shapes[i].cycle_length) {
        ac.shapes[i].cycle_length = abs(gcd(ac.shapes[i].cycle_length, at.eid - k));
        goto recompute;
        }
      sh.vertex_valence[k] = qty;
      sh.vertex_period[k] = pqty;
      sh.vertex_angles[k] = std::move(anglelist);
      }
    if(debugflags & DF_GEOM) 
      println(hlog, "computed vertex_valence of ", i, " as ", ac.shapes[i].vertex_valence);
    }
  
  ac.min_valence = UNKNOWN; ac.max_valence = 0;
  for(auto& sh: ac.shapes) 
    for(auto& val: sh.vertex_valence) {
      if(val < ac.min_valence) ac.min_valence = val;
      if(val > ac.max_valence) ac.max_valence = val;
      }      
  }

EX void load(const string& fname, bool after_sliding IS(false)) {
  fhstream f(fname, "rt");
  if(!f.f) throw hr_parse_exception("file " + fname + " does not exist");
  string s;
  while(true) {
    int c = fgetc(f.f);
    if(c < 0) break;
    s += c;
    }
  auto& c = after_sliding ? slided : current;
  c.order++;
  c.shapes.clear();
  c.sliders.clear();
  c.name = unnamed;
  c.comment = "";
  c.filename = fname;
  c.cscale = 1;
  c.range = 0;
  c.boundary_ratio = 1;
  c.floor_scale = .5;
  c.have_ph = c.have_line = false;
  c.have_tree = false;
  c.yendor_backsteps = 0;
  c.is_star = false;
  c.is_combinatorial = false;
  exp_parser ep;
  ep.s = s;
  ld angleunit = 1, distunit = 1, angleofs = 0;
  auto addflag = [&] (int f) {
    int ai;
    if(ep.next() == ')') ai = isize(c.shapes)-1;
    else ai = ep.iparse();
    verify_index(ai, c.shapes, ep); 
    c.shapes[ai].flags |= f;
    ep.force_eat(")");
    };
  while(true) {

    ep.extra_params["distunit"] = distunit;
    ep.extra_params["angleunit"] = angleunit;
    ep.extra_params["angleofs"] = angleofs;

    ep.skip_white();
    if(ep.next() == 0) break;
    if(ep.eat("#")) {
      bool doubled = ep.eat("#");
      while(ep.eat(" ")) ;
      string s = "";
      while(ep.next() >= 32) s += ep.next(), ep.at++;
      if(doubled) {
        if(c.name == unnamed) c.name = s;
        else {
          c.comment += s; 
          c.comment += "\n";
          }
        }
      }
    else if(ep.eat("e2.")) {
      ginf[gArbitrary].g = giEuclid2;
      ginf[gArbitrary].sides = 7;
      set_flag(ginf[gArbitrary].flags, qBOUNDED, false);
      set_flag(ginf[gArbitrary].flags, qAFFINE, false);
      geom3::apply_always3();
      }
    else if(ep.eat("a2.")) {
      ginf[gArbitrary].g = giEuclid2;
      ginf[gArbitrary].sides = 7;
      set_flag(ginf[gArbitrary].flags, qBOUNDED, false);
      set_flag(ginf[gArbitrary].flags, qAFFINE, true);
      affine_limit = 200;
      geom3::apply_always3();
      }
    else if(ep.eat("h2.")) {
      ginf[gArbitrary].g = giHyperb2;
      ginf[gArbitrary].sides = 7;
      set_flag(ginf[gArbitrary].flags, qBOUNDED, false);
      set_flag(ginf[gArbitrary].flags, qAFFINE, false);
      geom3::apply_always3();
      }
    else if(ep.eat("s2.")) {
      ginf[gArbitrary].g = giSphere2;
      ginf[gArbitrary].sides = 5;
      set_flag(ginf[gArbitrary].flags, qBOUNDED, true);
      set_flag(ginf[gArbitrary].flags, qAFFINE, false);
      geom3::apply_always3();
      }
    else if(ep.eat("legacysign.")) {
      if(legacy) angleunit *= -1;
      }
    else if(ep.eat("star.")) {
      c.is_star = true;
      }
    else if(ep.eat("combinatorial.")) {
      c.is_combinatorial = true;
      }
    else if(ep.eat("option(\"")) {
      next:
      string s = "";
      while(ep.next() != '"') s += ep.eatchar();
      ep.force_eat("\"");
      c.options.push_back(s);
      ep.skip_white();
      if(ep.eat(",")) { ep.skip_white(); ep.force_eat("\""); goto next; }
      ep.force_eat(")");
      }
    else if(ep.eat("angleunit(")) angleunit = real(ep.parsepar());
    else if(ep.eat("angleofs(")) {
      angleofs = real(ep.parsepar());
      if(!legacy) angleofs = 0;
      }
    else if(ep.eat("distunit(")) distunit = real(ep.parsepar());
    else if(ep.eat("line(")) {
      addflag(arcm::sfLINE);
      c.have_line = true;
      }
    else if(ep.eat("grave(")) {
      addflag(arcm::sfPH);
      c.have_ph = true;
      }
    else if(ep.eat("slider(")) {
      slider sl;
      sl.name = ep.next_token();
      ep.force_eat(",");
      sl.current = sl.zero = ep.rparse();
      ep.force_eat(",");
      sl.min = ep.rparse();
      ep.force_eat(",");
      sl.max = ep.rparse();
      ep.force_eat(")");
      c.sliders.push_back(sl);
      if(after_sliding)
        ep.extra_params[sl.name] = current.sliders[isize(c.sliders)-1].current;
      else
        ep.extra_params[sl.name] = sl.zero;
      }
    else if(ep.eat("let(")) {
      string tok = ep.next_token();
      ep.force_eat("=");
      ep.extra_params[tok] =ep.parsepar();
      if(debugflags & DF_GEOM)
        println(hlog, "let ", tok, " = ", ep.extra_params[tok]);
      }
    else if(ep.eat("unittile(")) load_tile(ep, c, true);
    else if(ep.eat("tile(")) load_tile(ep, c, false);
    else if(ep.eat("affine_limit(")) {
      affine_limit = ep.iparse();
      ep.force_eat(")");
      }
    else if(ep.eat("cscale(")) {
      c.cscale = ep.rparse();
      ep.force_eat(")");
      }
    else if(ep.eat("treestate(")) {
      rulegen::parse_treestate(c, ep);
      }
    else if(ep.eat("first_treestate(")) {
      rulegen::rule_root = ep.iparse();
      ep.force_eat(")");
      }
    else if(ep.eat("yendor_backsteps(")) {
      c.yendor_backsteps = ep.iparse();
      ep.force_eat(")");
      }
    else if(ep.eat("range(")) {
      c.range = ep.iparse();
      ep.force_eat(")");
      }
    else if(ep.eat("floor_scale(")) {
      c.floor_scale = ep.rparse();
      ep.force_eat(")");
      }
    else if(ep.eat("boundary_ratio(")) {
      c.boundary_ratio = ep.rparse();
      ep.force_eat(")");
      }
    else if(ep.eat("conway(\"")) {
      string s = "";
      while(true) {
        int m = 0;
        if(ep.eat("(")) m = 0;
        else if(ep.eat("[")) m = 1;
        else if(ep.eat("\"")) break;
        else throw hr_parse_exception("cannot parse Conway notation, " + ep.where());

        int ai = 0;
        int as = ep.iparse();
        while(ep.eat("'")) ai++;
        if(ep.eat("@")) ai = ep.iparse();
        int bi = 0, bs = 0;
        if(ep.eat(")") || ep.eat("]")) bs = as, bi = ai;
        else {
          bs = ep.iparse();
          while(ep.eat("'")) bi++;
          if(ep.eat("@")) bi = ep.iparse();
          }          
        if(ep.eat(")") || ep.eat("]")) {}
        verify_index(ai, c.shapes, ep);
        verify_index(as, c.shapes[ai], ep);
        verify_index(bi, c.shapes, ep);
        verify_index(bs, c.shapes[bi], ep);
        c.shapes[ai].connections[as] = connection_t{bi, bs, m};
        c.shapes[bi].connections[bs] = connection_t{ai, as, m};
        }
      ep.force_eat(")");
      }
    else if(ep.eat("c(")) {
      int ai = ep.iparse(); verify_index(ai, c.shapes, ep); ep.force_eat(",");
      int as = ep.iparse(); verify_index(as, c.shapes[ai], ep); ep.force_eat(",");
      int bi = ep.iparse(); verify_index(bi, c.shapes, ep); ep.force_eat(",");
      int bs = ep.iparse(); verify_index(bs, c.shapes[bi], ep); ep.force_eat(",");
      int m = ep.iparse(); ep.force_eat(")");
      c.shapes[ai].connections[as] = connection_t{bi, bs, m};
      c.shapes[bi].connections[bs] = connection_t{ai, as, m};
      }
    else if(ep.eat("subline(")) {
      int ai = ep.iparse(); verify_index(ai, c.shapes, ep); ep.force_eat(",");
      int as = ep.iparse(); verify_index(as, c.shapes[ai], ep); ep.force_eat(",");
      int bs = ep.iparse(); verify_index(bs, c.shapes[ai], ep); ep.force_eat(")");
      c.shapes[ai].sublines.emplace_back(as, bs);
      }
    else if(ep.eat("sublines(")) {
      ld d = ep.rparse() * distunit; 
      ld eps = 1e-4;
      if(ep.eat(",")) eps = ep.rparse() * distunit;
      ep.force_eat(")");
      for(auto& sh: c.shapes) {
        for(int i=0; i<isize(sh.vertices); i++)
        for(int j=0; j<i; j++)
          if(j != i+1 && i != j+1 && !(i==0 && j == isize(sh.vertices)-1) && !(j==0 && i == isize(sh.vertices)-1) && i != j) {
            ld dist = hdist(sh.vertices[i], sh.vertices[j]);
            if(abs(dist - d) < eps) {
              sh.sublines.emplace_back(i, j);
              if(debugflags & DF_GEOM) println(hlog, "add subline ", i, "-", j);
              }
            }
        }
      }
    else if(ep.eat("repeat(")) {
      int i = ep.iparse(0);
      verify_index(i, c.shapes, ep);
      ep.force_eat(",");
      int rep = ep.iparse(0);
      ep.force_eat(")");
      auto& sh = c.shapes[i];
      int N = isize(sh.angles);
      if(N % rep) 
        throw hr_parse_exception("repeat value should be a factor of the number of vertices, " + ep.where());
      sh.repeat_value = rep;
      
      int d = N / rep;
      for(int i=d; i<N; i++)
        sh.connections[i] = sh.connections[i-d];
      }
    else if(ep.eat("debug(")) {
      int i = ep.iparse(0);
      verify_index(i, c.shapes, ep);
      ep.force_eat(")");
      throw connection_debug_request(i);
      }
    else if(ep.eat("stretch_shear(")) {
      ld stretch = ep.rparse(0);
      ep.force_eat(",");
      ld shear = ep.rparse(0);
      ep.force_eat(",");
      int i = ep.iparse(0);
      verify_index(i, c.shapes, ep);
      ep.force_eat(",");
      int j = ep.iparse(0);
      verify_index(j, c.shapes[i], ep);
      ep.force_eat(")");
      auto& sh = c.shapes[i];
      sh.stretch_shear[j] = {stretch, shear};
      auto& co = sh.connections[j];
      auto& xsh = c.shapes[co.sid];
      ld scale = sh.edges[j] / xsh.edges[co.eid];
      println(hlog, "scale = ", scale);
      xsh.stretch_shear[co.eid] = {1/stretch, shear * (co.mirror ? 1 : -1) * stretch };
      }
    else throw hr_parse_exception("expecting command, " + ep.where());
    }
  if(!(cgflags & qAFFINE)) {
    for(int i=0; i<isize(c.shapes); i++) {
      auto& sh = c.shapes[i];
      for(int j=0; j<isize(sh.edges); j++) {
        ld d1 = sh.edges[j];
        auto con = sh.connections[j];
        auto& xsh = c.shapes[con.sid];
        ld d2 = xsh.edges[con.eid];
        if(abs(d1 - d2) > 1e-6)
          throw hr_parse_exception(lalign(0, "connecting ", make_pair(i,j), " to ", con, " of different lengths only possible in a2"));
        }
      }
    }

  if(do_unmirror) {
    unmirror();
    }
  if(!c.have_tree) compute_vertex_valence();

  if(c.have_tree) rulegen::verify_parsed_treestates();
  
  if(!after_sliding) slided = current;
  }

arbi_tiling debugged;
vector<pair<transmatrix, int> > debug_polys;

string primes(int i) {
  string res;
  while(i--) res += "'";
  return res;
  }

void connection_debugger() {
  cmode = sm::SIDE | sm::DIALOG_STRICT_X;
  gamescreen(0);
  
  auto& last = debug_polys.back();
  
  initquickqueue();
  for(auto& p: debug_polys) {
    int id = p.second;
    
    shiftmatrix V = gmatrix[cwt.at] * p.first;
    
    auto& sh = debugged.shapes[id].vertices;
    
    for(auto& v: sh)
      curvepoint(v);

    curvepoint(sh[0]);
    
    color_t col = colortables['A'][id];
    col = darkena(col, 0, 0xFF);
    
    if(&p == &last) {
      vid.linewidth *= 2;
      queuecurve(V, 0xFFFF00FF, col, PPR::LINE);
      vid.linewidth /= 2;
      for(int i=0; i<isize(sh); i++)
        queuestr(V * sh[i], vid.fsize, its(i), 0xFFFFFFFF);
      }
    else
      queuecurve(V, 0xFFFFFFFF, col, PPR::LINE);
    }
  quickqueue();

  dialog::init(XLAT("connection debugger"));
  
  dialog::addInfo(debugged.name);
  dialog::addHelp(debugged.comment);
  
  dialog::addBreak(50);

  dialog::addInfo("face index " + its(last.second));

  dialog::addBreak(50);  
  
  auto& sh = debugged.shapes[last.second];
  int N = isize(sh.edges);
  for(int k=0; k<N; k++) {
    auto con = sh.connections[k];
    string cap = its(k) + primes(last.second) + " -> " + its(con.eid) + primes(con.sid) + (con.mirror ? " (m) " : "");
    dialog::addSelItem(cap, "go", '0' + k);
    
    dialog::add_action([k, last, con] {
      if(euclid) cgflags |= qAFFINE;
      debug_polys.emplace_back(last.first * get_adj(debugged, last.second, k, -1, -1), con.sid);
      if(euclid) cgflags &= ~qAFFINE;
      });
    
    }    

  dialog::addItem("undo", 'u');
  dialog::add_action([] {
    if(isize(debug_polys) > 1)
      debug_polys.pop_back();
    });
  
  dialog::addBack();
  dialog::display();

  keyhandler = [] (int sym, int uni) {
    handlePanning(sym, uni);
    dialog::handleNavigation(sym, uni);
    if(doexiton(sym, uni)) popScreen();
    };
  }

geometryinfo1& arbi_tiling::get_geometry() {
  return ginf[gEuclid].g;
  }

map<heptagon*, vector<pair<heptagon*, transmatrix> > > altmap;

EX map<heptagon*, pair<heptagon*, transmatrix>> arbi_matrix;

EX hrmap *current_altmap;

heptagon *build_child(heptspin p, pair<int, int> adj);

EX transmatrix get_adj(arbi_tiling& c, int t, int dl, int t1, int xdl) {
  
  auto& sh = c.shapes[t];
  
  int dr = gmod(dl+1, sh.size());

  auto& co = sh.connections[dl];
  if(xdl == -1) xdl = co.eid;

  if(t1 == -1) t1 = co.sid;

  auto& xsh = c.shapes[t1];
  int xdr = gmod(xdl+1, xsh.size());

  hyperpoint vl = sh.vertices[dl];
  hyperpoint vr = sh.vertices[dr];
  hyperpoint vm = mid(vl, vr);
      
  transmatrix rm = gpushxto0(vm);
  
  hyperpoint xvl = xsh.vertices[xdl];
  hyperpoint xvr = xsh.vertices[xdr];
  hyperpoint xvm = mid(xvl, xvr);
  
  transmatrix xrm = gpushxto0(xvm);
  
  transmatrix Res = rgpushxto0(vm) * rspintox(rm*vr);
    
  if(cgflags & qAFFINE) {
    ld sca = hdist(vl, vr) / hdist(xvl, xvr);
    transmatrix Tsca = Id;
    Tsca[0][0] = Tsca[1][1] = sca;

    auto& ss = sh.stretch_shear[dl];
    Tsca[0][1] = ss.first * ss.second * sca;
    Tsca[1][1] *= ss.first;
    
    Res = Res * Tsca;
    }

  if(co.mirror) Res = Res * MirrorX;
  Res = Res * spintox(xrm*xvl) * xrm;
  
  if(co.mirror) swap(vl, vr);
  
  if(hdist(vl, Res*xvr) + hdist(vr, Res*xvl) > .1 && !c.is_combinatorial) {
    println(hlog, "s1 = ", kz(spintox(rm*vr)), " s2 = ", kz(rspintox(xrm*xvr)));    
    println(hlog, tie(t, dl), " = ", kz(Res));    
    println(hlog, hdist(vl, Res * xvr), " # ", hdist(vr, Res * xvl));
    exit(3);
    }
        
  return Res;
  }

struct hrmap_arbi : hrmap {
  heptagon *origin;
  heptagon *getOrigin() override { return origin; }

  hrmap_arbi() {
    dynamicval<hrmap*> curmap(currentmap, this);
    origin = init_heptagon(current.shapes[0].size());
    origin->s = hsOrigin;
    origin->c7 = newCell(origin->type, origin);

    heptagon *alt = NULL;
    
    if(hyperbolic) {
      dynamicval<eGeometry> g(geometry, gNormal); 
      alt = init_heptagon(S7);
      alt->s = hsOrigin;
      alt->alt = alt;
      current_altmap = newAltMap(alt); 
      }
    
    transmatrix T = xpush(.01241) * spin(1.4117) * xpush(0.1241) * Id;
    arbi_matrix[origin] = make_pair(alt, T);
    altmap[alt].emplace_back(origin, T);
    
    if(!current.range)
      current.range = auto_compute_range(origin->c7);
    }

  ~hrmap_arbi() {
    clearfrom(origin);
    altmap.clear();
    arbi_matrix.clear();
    if(current_altmap) {
      dynamicval<eGeometry> g(geometry, gNormal);       
      delete current_altmap;
      current_altmap = NULL;
      }
    }
  void verify() override { }

  transmatrix adj(heptagon *h, int dl) override { 
    return get_adj(current_or_slided(), id_of(h), dl, -1, h->c.move(dl) ? h->c.spin(dl) : -1);
    }

  heptagon *create_step(heptagon *h, int d) override {
  
    dynamicval<bool> sl(using_slided, false);
    int t = id_of(h);
  
    auto& sh = current.shapes[t];
    
    auto& co = sh.connections[d];
    
    auto& xsh = current.shapes[co.sid];
    
    if(cgflags & qAFFINE) {
      set<heptagon*> visited;
      
      vector<pair<heptagon*, transmatrix> > v;
      
      visited.insert(h);
      v.emplace_back(h, Id);
      
      transmatrix goal = adj(h, d);
      
      for(int i=0; i<affine_limit && i < isize(v); i++) {
        transmatrix T = v[i].second;
        heptagon *h2 = v[i].first;
        if(eqmatrix(T, goal)) {
          h->c.connect(d, h2, co.eid, co.mirror);
          return h2;
          }
        for(int i=0; i<h2->type; i++) {
          heptagon *h3 = h2->move(i);
          if(!h3) continue;
          if(visited.count(h3)) continue;
          visited.insert(h3);
          v.emplace_back(h3, T * adj(h2, i));
          }
        }
      
      auto h1 = init_heptagon(current.shapes[co.sid].size());
      h1->distance = h->distance + 1;
      h1->zebraval = co.sid;
      h1->c7 = newCell(h1->type, h1);
      h1->emeraldval = h->emeraldval ^ co.mirror;
      h->c.connect(d, h1, co.eid, co.mirror);
      
      return h1;
      }

    const auto& p = arbi_matrix[h];
    
    heptagon *alt = p.first;
    
    transmatrix T = p.second * adj(h, d);
    
    if(hyperbolic) {
      dynamicval<eGeometry> g(geometry, gNormal); 
      dynamicval<hrmap*> cm(currentmap, current_altmap);
      // transmatrix U = T;
      current_altmap->virtualRebase(alt, T);
      // U = U * inverse(T);
      }
    fixmatrix(T);
    
    if(euclid) {
      /* hash the rough coordinates as heptagon* alt */
      size_t s = size_t(T[0][LDIM]+.261) * 124101 + size_t(T[1][LDIM]+.261) * 82143;
      alt = (heptagon*) s;
      }

    for(auto& p2: altmap[alt]) if(id_of(p2.first) == co.sid && same_point_may_warn(tC0(p2.second), tC0(T))) {
      for(int oth=0; oth < p2.first->type; oth++) {
        if(same_point_may_warn(p2.second * xsh.vertices[oth], T * xsh.vertices[co.eid])) {
          h->c.connect(d, p2.first, oth%p2.first->type, co.mirror);
          return p2.first;
          }
        }
      }

    auto h1 = init_heptagon(current.shapes[co.sid].size());
    h1->distance = h->distance + 1;
    h1->zebraval = co.sid;
    h1->c7 = newCell(h1->type, h1);
    h1->emeraldval = h->emeraldval ^ co.mirror;
    h->c.connect(d, h1, co.eid, co.mirror);
    
    arbi_matrix[h1] = make_pair(alt, T);
    altmap[alt].emplace_back(h1, T);    
    return h1;
    }
  
  transmatrix relative_matrixh(heptagon *h2, heptagon *h1, const hyperpoint& hint) override {
    return relative_matrix_recursive(h2, h1);
    }

  transmatrix adj(cell *c, int dir) override { return adj(c->master, dir); }
  
  ld spin_angle(cell *c, int d) override { return SPIN_NOT_AVAILABLE; }

  int shvid(cell *c) override {
    return id_of(c->master);
    }

  hyperpoint get_corner(cell *c, int cid, ld cf) override {
    auto& sh = arb::current_or_slided().shapes[arb::id_of(c->master)];
    return normalize(C0 + (sh.vertices[gmod(cid, c->type)] - C0) * 3 / cf);
    }

  };

EX hrmap *new_map() { return new hrmap_arbi; }

EX void run(string fname) {
  stop_game();
  eGeometry g = geometry;
  arbi_tiling t = current;
  auto v = variation;
  set_geometry(gArbitrary);
  try {
     load(fname);
     ginf[gArbitrary].tiling_name = current.name;
     }
   catch(hr_polygon_error& poly) {
     set_geometry(g);
     set_variation(v);
     current = t;
     start_poly_debugger(poly);     
     string help = poly.generate_error();
     showstartmenu = false;
     for(auto& p: poly.params)
       help += lalign(-1, p.first, " = ", p.second, "\n");
     gotoHelp(help);
     }
   catch(hr_parse_exception& ex) {
     println(hlog, "failed: ", ex.s);
     set_geometry(g);
     current = t;
     start_game();
     addMessage("failed: " + ex.s);
     }
  catch(connection_debug_request& cr) {
    set_geometry(g);     
    debugged = current;
    current = t;
    ensure_geometry(cr.c);
    debug_polys.clear();
    debug_polys.emplace_back(Id, cr.id);
    pushScreen(connection_debugger);
    }
  start_game();
  }

string slider_error;

EX void sliders_changed() {
  try {
    load(current.filename, true);
    using_slided = true;
    slider_error = "OK";
    #if CAP_TEXTURE
    texture::config.remap();
    #endif
    }
  catch(hr_parse_exception& ex) {
    using_slided = false;
    slider_error = ex.s;
    }
  catch(hr_polygon_error& poly) {
    using_slided = false;
    slider_error = poly.generate_error();
    }
  }

EX void set_sliders() {
  cmode = sm::SIDE | sm::MAYDARK;
  gamescreen(1);
  dialog::init(XLAT("tessellation sliders"));
  char ch = 'A';
  for(auto& sl: current.sliders) {
    dialog::addSelItem(sl.name, fts(sl.current), ch++);
    dialog::add_action([&] {
      dialog::editNumber(sl.current, sl.min, sl.max, 1, sl.zero, sl.name, sl.name);
      dialog::reaction = sliders_changed;
      });
    }
  dialog::addInfo(slider_error);
  dialog::addBack();
  dialog::display();
  }

/** convert a tessellation (e.g. Archimedean, regular, etc.) to the arb::current internal representation */
EX namespace convert {

EX eGeometry base_geometry;
EX eVariation base_variation;

struct id_record {
  int target;   /* master of this id type */
  int shift;    /* sample direction 0 == our direction shift */
  int modval;   /* this master type is the same as itself rotated by modval */
  cell *sample; /* sample of the master type */
  };

inline void print(hstream& hs, const id_record& i) { print(hs, "[", i.target, " shift=", i.shift, " mod=", i.modval, "]"); }

map<int, id_record> identification;

id_record& get_identification(int s, cell *c) {  
  if(!identification.count(s)) {
    auto &id = identification[s];
    id.target = s;
    id.shift = 0;
    id.modval = c->type;
    id.sample = c;
    }
  return identification[s];
  }

id_record& get_identification(cell *c) {  
  auto id = currentmap->full_shvid(c);
  return get_identification(id, c);
  }

int changes;

void be_identified(cellwalker cw1, cellwalker cw2) {
  auto& id1 = get_identification(cw1.at);
  auto& id2 = get_identification(cw2.at);
  
  indenter ind(2);
  
  int t = cw2.at->type;

  if(cw1.at->type != t) {
    println(hlog, cw1.at->type, " vs ", t);
    throw hr_exception("numbers disagree");
    }
    
  int d2 = gmod(-cw2.to_spin(id2.shift), id2.modval);
  int d1 = gmod(-cw1.to_spin(id1.shift), id1.modval);

  indenter ind1(2);

  if(id2.target != id1.target) {
    auto oid2 = id2;
    id1.modval = gcd(id1.modval, id2.modval);
    for(auto& p: identification) {
      auto& idr = p.second;
      if(idr.target == oid2.target) {
        idr.target = id1.target;
        idr.modval = id1.modval;
        idr.shift = gmod(idr.shift + (d2-d1), idr.modval);        
        idr.sample = id1.sample;
        }
      }
    changes++;
    println(hlog, identification);
    return;
    }
  if(d2 != d1) {
    auto oid2 = id2;
    id2.modval = gcd(id2.modval, abs(d2-d1));
    for(auto& p: identification) 
      if(p.second.target == oid2.target) p.second.modval = id2.modval;
    changes++;
    println(hlog, identification);
    return;
    }
  }

EX void convert() {
  start_game();
  identification.clear(); changes = 0;

  manual_celllister cl;
  cl.add(currentmap->gamestart());
  
  int more_tests = 1000;
  pointer_indices.clear();

  int chg = -1;
  while(changes > chg) {
    changes = chg;
    
    set<int> masters_analyzed;

    for(int i=0; i<isize(cl.lst); i++) {
      auto c = cl.lst[i];
      auto& id = get_identification(c);
      
      if(masters_analyzed.count(id.target)) {
        more_tests--;
        if(more_tests < 0) continue;
        }
      masters_analyzed.insert(id.target);
      
      cellwalker cw0(c, id.shift);
      cellwalker cws(id.sample, 0);
      
      for(int i=0; i<c->type; i++) {
        if(1) {
          indenter ind(2);
          be_identified(cw0 + i + wstep, cws + i + wstep);
          be_identified(cw0 + i + wstep, cw0 + i + id.modval + wstep);
          }
        
        if(1) {
          indenter ind(2);
          auto cwx = cw0 + i + wstep;
          
          auto idx = get_identification(cwx.at);
          cellwalker xsample(idx.sample, cwx.spin);
          xsample -= idx.shift;
          
          be_identified(cwx + wstep, xsample + wstep);
          
          cl.add((cw0 + i).cpeek());
          }
        }
      }
    }
  
  vector<int> old_shvids;
  map<int, int> old_to_new;
  for(auto id: identification)
    if(id.first == id.second.target) {
      old_to_new[id.first] = isize(old_shvids);
      old_shvids.push_back(id.first);
      }
  
  auto& ac = arb::current;
  ac.order++; 
  ac.comment = ac.filename = "converted from: " + full_geometry_name();
  ac.cscale = cgi.scalefactor;
  ac.boundary_ratio = 1;
  ac.floor_scale = cgi.hexvdist / cgi.scalefactor;
  ac.range = cgi.base_distlimit;
  int N = isize(old_shvids);
  ac.shapes.clear();
  ac.shapes.resize(N);

  ginf[gArbitrary].g = cginf.g;
  ginf[gArbitrary].flags = cgflags & qBOUNDED;
  
  for(int i=0; i<N; i++) {
    auto id = identification[old_shvids[i]];
    cell *s = id.sample;
    auto& sh = ac.shapes[i];
    sh.id = i;
    int t = s->type;
    sh.vertices.clear();
    sh.connections.clear();
    sh.repeat_value = id.modval;
    for(int j=0; j<t; j++) {
      auto co = currentmap->get_corner(s, j);
      sh.vertices.push_back(co);
      cellwalker cw(s, j);
      cw += wstep;
      auto idx = get_identification(cw.at);
      cellwalker xsample(idx.sample, cw.spin);
      xsample -= idx.shift;
      sh.connections.emplace_back(arb::connection_t{old_to_new.at(idx.target), xsample.spin, false});
      }
    sh.stretch_shear.resize(t, make_pair(1, 0));    
    sh.edges.clear();
    for(int j=0; j<t-1; j++)
      sh.edges.push_back(hdist(sh.vertices[j], sh.vertices[j+1]));
    sh.edges.push_back(hdist(sh.vertices[t-1], sh.vertices[0]));

    sh.angles.clear();
    for(int j=0; j<t; j++) {
      hyperpoint v0 = sh.vertices[j];
      hyperpoint v1 = sh.vertices[(j+1) % t];
      hyperpoint v2 = sh.vertices[(j+2) % t];
      transmatrix T = gpushxto0(v1);
      v0 = T * v0;
      v2 = T * v2;
      ld alpha = atan2(v0) - atan2(v2);
      while(alpha > M_PI) alpha -= 360*degree;
      while(alpha < -M_PI) alpha += 360*degree;
      sh.angles.push_back(alpha);
      }
    if(debugflags & DF_GEOM) {
      println(hlog, "shape ", i, ":");
      indenter indp(2);
      println(hlog, "vertices=", sh.vertices);
      println(hlog, "connections=", sh.connections);
      println(hlog, "edges=", sh.edges);
      println(hlog, "angles=", sh.angles);
      }
    }
  
  arb::compute_vertex_valence();
  }

EX bool in() {
  return arb::in() && base_geometry != gArbitrary;
  }

/** activate the converted tessellation */
EX void activate() {
  if(geometry != gArbitrary) {
    base_geometry = geometry;
    base_variation = variation;
    stop_game();
    geometry = gArbitrary;
    variation = eVariation::pure;
    }
  }

EX }

#if CAP_COMMANDLINE  
int readArgs() {
  using namespace arg;
           
  if(0) ;
  else if(argis("-tes") || argis("-arbi")) {
    PHASEFROM(2);
    shift(); 
    run(args());
    }
  else if(argis("-tes-opt")) {
     arg::run_arguments(current.options);
    }
  else if(argis("-arb-legacy")) {
    legacy = true;
    }
  else if(argis("-arb-convert")) {
    try {
      convert::convert();
      set_geometry(gArbitrary);      
      }
    catch(hr_exception& e) {
      println(hlog, "failed to convert: ", e.what());
      }
    }
  else if(argis("-arb-slider")) {
    PHASEFROM(2);
    shift();
    string slider = args();
    bool found = true;
    for(auto& sl: current.sliders)
      if(sl.name == slider) {
        shift_arg_formula(sl.current, sliders_changed);
        found = true;
        }
    if(!found) {
      println(hlog, "warning: no slider named ", slider, " found");
      shift();
      }
    }
  else return 1;
  return 0;
  }

auto hook = addHook(hooks_args, 100, readArgs);
#endif

EX bool in() { return geometry == gArbitrary; }

EX string tes = "tessellations/sample/marjorie-rice.tes";

EX bool linespattern(cell *c) {
  return current.shapes[id_of(c->master)].flags & arcm::sfLINE;
  }

EX bool pseudohept(cell *c) {
  if(!current.have_ph) return true;
  return current.shapes[id_of(c->master)].flags & arcm::sfPH;
  }

EX void choose() {
  dialog::openFileDialog(tes, XLAT("open a tiling"), ".tes", 
  [] () {
    run(tes);
    if(!current.options.empty())
      dialog::push_confirm_dialog([] { arg::run_arguments(current.options); start_game(); }, "load the settings defined in this file?");
    return true;
    });
  }

#if MAXMDIM >= 4
auto hooksw = addHook(hooks_swapdim, 100, [] {
  for(auto& p: {&current, &slided}) 
    for(auto& s: p->shapes)
      for(auto& v: s.vertices)
        swapmatrix(v);
  for(auto& p: altmap) for(auto& pp: p.second) swapmatrix(pp.second);
  for(auto& p: arbi_matrix) swapmatrix(p.second.second);
  });
#endif

EX }
}
