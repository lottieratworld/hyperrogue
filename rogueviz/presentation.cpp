#ifndef PRESENTATION_CPP
#define PRESENTATION_CPP

#include "../rogueviz/rogueviz.h"

namespace rogueviz {

#if CAP_RVSLIDES
namespace pres {

/* maks graphs in presentations */
grapher::grapher(ld _minx, ld _miny, ld _maxx, ld _maxy) : minx(_minx), miny(_miny), maxx(_maxx), maxy(_maxy) {
  auto& cd = *current_display;
  
  ld xpixels = 2 * min(cd.xcenter - cd.xmin, cd.xmax - cd.xcenter);
  ld ypixels = 2 * min(cd.ycenter - cd.ymin, cd.ymax - cd.ycenter);
  
  ld sca = min(abs(xpixels / (maxx-minx)), abs(ypixels / (maxy-miny)));
  
  ld medx = (minx + maxx) / 2;
  ld medy = (miny + maxy) / 2;

  hyperpoint zero = atscreenpos(cd.xcenter - sca * medx, cd.ycenter + sca * medy, 1) * C0;

  hyperpoint zero10 = atscreenpos(cd.xcenter - sca * medx + sca, cd.ycenter + sca * medy, 1) * C0;
  hyperpoint zero01 = atscreenpos(cd.xcenter - sca * medx, cd.ycenter + sca * medy - sca, 1) * C0;
  
  T = shiftless(Id);
  T.T[LDIM] = zero;
  T.T[0] = zero10 - zero;
  T.T[1] = zero01 - zero;
  
  T.T = transpose(T.T);
  }

void grapher::line(hyperpoint h1, hyperpoint h2, color_t col) {
    curvepoint(h1);
    curvepoint(h2);
    queuecurve(T, col, 0, PPR::LINE).flags |= POLY_FORCEWIDE;
    }
  
void grapher::arrow(hyperpoint h1, hyperpoint h2, ld sca, color_t col) {
  line(h1, h2, col);
  if(!sca) return;
  hyperpoint h = h2 - h1;
  ld siz = hypot_d(2, h);
  h *= sca / siz;
  curvepoint(h2);
  curvepoint(h2 - spin(15*degree) * h);
  curvepoint(h2 - spin(-15*degree) * h);
  curvepoint(h2);
  queuecurve(T, col, col, PPR::LINE);
  }
  
shiftmatrix grapher::pos(ld x, ld y, ld sca) {
  transmatrix P = Id;
  P[0][0] = sca;
  P[1][1] = sca;
  P[0][LDIM] = x;
  P[1][LDIM] = y;
  return T * P;
  }

hyperpoint p2(ld x, ld y) { return LDIM == 2 ? point3(x, y, 1) : point31(x, y, 0); }

/* temporary hooks */

using namespace hr::tour;

void add_stat(presmode mode, const bool_reaction_t& stat) {
  add_temporary_hook(mode, hooks_prestats, 200, stat);
  }

void no_other_hud(presmode mode) {
  add_temporary_hook(mode, hooks_prestats, 300, [] { return true; });
  clearMessages();
  }

void empty_screen(presmode mode, color_t col) {
  if(mode == pmStart) {
    tour::slide_backup(nomap, true);
    tour::slide_backup(backcolor, col);
    tour::slide_backup(ringcolor, color_t(0));
    tour::slide_backup<color_t>(dialog::dialogcolor, 0);
    tour::slide_backup<color_t>(forecolor, 0);
    tour::slide_backup<color_t>(bordcolor, 0xFFFFFFFF);
    tour::slide_backup(vid.aurastr, 0);
    }
  }

void slide_error(presmode mode, string s) {
  empty_screen(mode, 0x400000);
  add_stat(mode, [s] {
    dialog::init();
    dialog::addTitle(s, 0xFF0000, 150);
    dialog::display();
    return true;
    });
  }

map<string, texture::texture_data> textures;

void draw_texture(texture::texture_data& tex) {
  static vector<glhr::textured_vertex> rtver(4);
  
  int fs = inHighQual ? 0 : 2 * vid.fsize;
  
  ld tx = tex.tx;
  ld ty = tex.ty;
  ld scalex = (vid.xres/2 - fs) / (current_display->radius * tx);
  ld scaley = (vid.yres/2 - fs) / (current_display->radius * ty);
  ld scale = min(scalex, scaley);
  scale *= 2;

  for(int i=0; i<4; i++) {
    ld cx[4] = {1,0,0,1};
    ld cy[4] = {1,1,0,0};
    rtver[i].texture[0] = (tex.base_x + (cx[i] ? tex.strx : 0.)) / tex.twidth;
    rtver[i].texture[1] = (tex.base_y + (cy[i] ? tex.stry : 0.)) / tex.theight;
    rtver[i].coords[0] = (cx[i]*2-1) * scale * tx;
    rtver[i].coords[1] = (cy[i]*2-1) * scale * ty;
    rtver[i].coords[2] = 1;
    rtver[i].coords[3] = 1;
    }
  
  glhr::be_textured();
  current_display->set_projection(0, false);
  glBindTexture(GL_TEXTURE_2D, tex.textureid);
  glhr::color2(0xFFFFFFFF);
  glhr::id_modelview();
  current_display->set_mask(0);
  glhr::prepare(rtver);
  glhr::set_depthtest(false);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  }
  
void show_picture(presmode mode, string s) {
  if(mode == pmStartAll) {
    auto& tex = textures[s];
    println(hlog, "rt = ", tex.readtexture(s));
    println(hlog, "gl = ", tex.loadTextureGL());
    }
  add_stat(mode, [s] {
    auto& tex = textures[s];
    flat_model_enabler fme;    
    draw_texture(tex);
    return false;
    });
  }

int video_start = 0;

void read_all(int fd, void *buf, int cnt) {
  char *cbuf = (char*) buf;
  while(cnt > 0) {
    int qt = read(fd, cbuf, cnt);
    if(qt <= 0) break;
    cbuf += qt;
    cnt -= qt;
    }
  }

/* note: this loads the whole animation uncompressed into memory, so it is suitable only for short presentations */
void show_animation(presmode mode, string s, int sx, int sy, int frames, int fps) {
#if CAP_VIDEO
  if(mode == pmStartAll) {
    array<int, 2> tab;
    if(pipe(&tab[0])) {
      addMessage(format("Error: %s", strerror(errno)));
      return;
      }

    int pid = fork();
    fflush(stdout);

    fprintf(stderr, "pipe is %d:%d\n", tab[0], tab[1]);

    if(pid == 0) {
      fprintf(stderr, "in child\n");
     fprintf(stderr, "making fformat\n");
      string fformat = "ffmpeg -y -i " + s + " -f rawvideo -pix_fmt bgra /dev/fd/" + its(tab[1]);    
      int sys = system(fformat.c_str());
      ::close(tab[0]);
      fprintf(stderr, "system call returned %d: %s\n", sys, strerror(errno));
      ::close(tab[1]);
      exit(0);
      }
      
    ::close(tab[1]);
    for(int i=0; i<frames; i++) {
      auto& tex = textures[s + "@" + its(i)];
      tex.strx = tex.tx = sx;
      tex.stry = tex.ty = sy;
      tex.twidth = next_p2(tex.tx);
      tex.theight = next_p2(tex.ty);
      tex.base_x = tex.base_y = 0;
      tex.texture_pixels.resize(tex.twidth * tex.theight);
      
      for(int y=0; y<sy; y++) {
        read_all(tab[0], &tex.texture_pixels[tex.twidth * y], 4 * sx);
        }
      
      println(hlog, "load frame ", i, " = ", tex.loadTextureGL(), " color = ", tex.texture_pixels[0]);
      // tex.loadTextureGL();
      }
    
    ::close(tab[0]);
    println(hlog, "waiting");
    wait(nullptr);
    println(hlog, "waited");
    video_start = ticks;
    }
  add_stat(mode, [s, frames, fps] {
    int f = (ticks - video_start) / 1000. * fps;
    f %= frames;
    auto& tex = textures[s + "@" + its(f)];
    flat_model_enabler fme;    
    draw_texture(tex);
    return false;
    });
#endif
  }

void choose_presentation() {
  gamescreen(2);

  getcstat = ' ';
  
  dialog::init(XLAT("presentations"), 0xFFD500);

  ss::for_all_slideshows([] (string title, slide *sl, char ch) {
    dialog::addItem(title, ch);
    dialog::add_action([sl] { 
      tour::slides = sl;
      if(!tour::texts) nomenukey = true;
      popScreenAll();
      tour::start();
      });
    });
    
  dialog::addBreak(100);
  
  dialog::addBoolItem_action(XLAT("enable/disable texts"), tour::texts, '7');
  
  dialog::display();
  }

int phooks = 
  0
  + addHook(dialog::hooks_display_dialog, 100, [] () {
    if(current_screen_cfunction() == showStartMenu) { 
      dialog::addBreak(100);
      dialog::addBigItem(XLAT("RogueViz demos"), 'p');
      dialog::add_action([] () { pushScreen(choose_presentation); });
      }
    });

void use_angledir(presmode mode, bool reset) {
  if(mode == pmStart && reset)
    angle = 0, dir = -1;
  add_temporary_hook(mode, shmup::hooks_turn, 200, [] (int i) {
    angle += dir * i / 500.;
    if(angle > M_PI/2) angle = M_PI/2;
    if(angle < 0) angle = 0;
    return false;
    });
  
  if(mode == pmKey) dir = -dir;
  }

void compare_projections(presmode mode, eModel a, eModel b) {
  static function<void()> w;
  if(mode == pmStart) {
    w = wrap_drawfullmap;
    tour::slide_backup(wrap_drawfullmap, w);
    wrap_drawfullmap = [a, b] {
      if(1) {
        dynamicval<ld> xmin(current_display->xmin, 0);
        dynamicval<ld> xmax(current_display->xmax, 0.49);
        dynamicval<eModel> pm(pmodel, a);
        calcparam();
        w();
        current_display->xmin = .51;
        current_display->xmax = 1;
        pmodel = b;
        calcparam();
        w();
        }
      calcparam();
      };
    }
  }

/* default RogueViz tour */

vector<slide> rvslides_mixed;
vector<slide> rvslides_data;
extern vector<slide> rvslides_default;

void add_end(vector<slide>& s) {
  s.emplace_back(
    slide{"THE END", 99, LEGAL::NONE | FINALSLIDE,
    "Press '5' to leave the presentation.",
    [] (presmode mode) {
      if(mode == pmStart) firstland = specialland = laIce;
      if(mode == 4) restart_game(rg::tour);
      }
    });
  }
  
slide *gen_rvtour_data() {
  rvslides_data = rvslides_default;

  callhooks(hooks_build_rvtour, "data", rvslides_data);
  add_end(rvslides_data);

  return &rvslides_data[0];
  }

slide *gen_rvtour_mixed() {

  rvslides_mixed.emplace_back(slide{
    "RogueViz", 999, LEGAL::ANY,
    "This presentation is mostly composed from various unsorted demos, mostly posted on Twitter and YouTube. Press Enter to continue, ESC to look at other functions of this presentation.",
    [] (presmode mode) {
      slide_url(mode, 'y', "YouTube link", "https://www.youtube.com/user/ZenoTheRogue");
      slide_url(mode, 't', "Twitter link", "https://twitter.com/zenorogue/");
      }
    });
  
  callhooks(hooks_build_rvtour, "mixed", rvslides_mixed); 

  add_end(rvslides_mixed);

  return &rvslides_mixed[0];
  }

vector<slide> rvslides_default = {
    {"intro", 999, LEGAL::ANY, 
      "Hyperbolic space is great "
      "for visualizing some kinds of data because of the vast amount "
      "of space.\n\n"
      "Press '5' to switch to the standard HyperRogue tutorial. "
      "Press ESC to look at other functions of this presentation."
      ,
      [] (presmode mode) {
        slidecommand = "the standard presentation";
        if(mode == pmStartAll) firstland = specialland = laPalace;
        if(mode == 4) {
          tour::slides = default_slides;
          while(tour::on) restart_game(rg::tour);
          firstland = specialland = laIce;
          tour::start();
          }
        }
      },
    {"straight lines in the Palace", 999, LEGAL::ANY, 
      "One simple slide about HyperRogue. Press '5' to show some hyperbolic straight lines.",
      [] (presmode mode) {
       using namespace linepatterns;
       slidecommand = "toggle the Palace lines";
       if(mode == 4) patPalace.color = (patPalace.color == 0xFFD500FF ? 0 : 0xFFD500FF);
       if(mode == 3) patPalace.color = 0xFFD50000;
        }
      },
  };

int pres_hooks = 
  addHook(hooks_slide, 100, [] (int mode) {
    if(currentslide == 0 && slides == default_slides) {
      slidecommand = "RogueViz presentation";
      if(mode == 1)
        help += 
          "\n\nYour version of HyperRogue is compiled with RogueViz. "
          "Press '5' to switch to the RogueViz slides. Watching the "
          "common HyperRogue tutorial first is useful too, "
          "as an introduction to hyperbolic geometry.";         
      if(mode == 4) {
        while(tour::on) restart_game(rg::tour);
        pushScreen(choose_presentation);
        }
      }
    }) +
  addHook(dialog::hooks_display_dialog, 100, [] () {
    if(current_screen_cfunction() == showMainMenu) {
      dialog::addItem(XLAT("RogueViz demos"), 'd'); 
      dialog::add_action_push(choose_presentation);
      }
    }) +
  addHook_slideshows(300, [] (tour::ss::slideshow_callback cb) {
    if(rogueviz::pres::rvslides_data.empty()) pres::gen_rvtour_data();
    cb(XLAT("non-Euclidean geometry in data analysis"), &pres::rvslides_data[0], 'd');

    if(rogueviz::pres::rvslides_mixed.empty()) pres::gen_rvtour_mixed();

    cb(XLAT("unsorted RogueViz demos"), &pres::rvslides_mixed[0], 'u');
    }) +
  0;

}
#endif
}

#endif
