/* render.c — WebGL2 Pong (WASM) — full parity with Pygame version
 * Features:
 *  - States: MENU, PLAY, GAME_OVER
 *  - 1P/2P toggle with UP/DOWN in MENU; SPACE to start / return
 *  - P1 controls: A/Z or ArrowUp/ArrowDown;  P2: K/M (in 2P)
 *  - AI paddle mirrors original blend-target logic
 *  - Ripple VFX on hits/walls; paddle flash; dashed center line
 *  - Score to 10; HUD via DOM overlay (mode, scores, prompts)
 *  - WebAudio SFX loading & playback using Emscripten FS (preload @ /sounds)
 *  - Optional music: /music/theme.ogg if present
 *
 * Build note: this file uses EM_ASM/EM_JS. Compile as -std=gnu99.
 */

#include <GLES3/gl3.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---------------------------- Config / Colors ---------------------------- */
static const int WIDTH  = 800;
static const int HEIGHT = 480;

static const float PLAYER_SPEED  = 6.0f;
static const float MAX_AI_SPEED  = 6.0f;

static const float WHITE[4]  = {1.0f, 1.0f, 1.0f, 1.0f};
static const float GREEN[4]  = {30/255.0f, 128/255.0f, 30/255.0f, 1.0f};
static const float YELLOW[4] = {240/255.0f,240/255.0f, 50/255.0f,1.0f};
static const float RED[4]    = {240/255.0f, 50/255.0f, 50/255.0f,1.0f};
static const float BLUE[4]   = { 50/255.0f, 50/255.0f,240/255.0f,1.0f};

/* ------------------------------ GL Program ------------------------------ */
static GLuint prog = 0;
static GLint  uResolution, uScale, uTranslate, uColor;

static GLuint vaoRect = 0, vboRect = 0;  /* unit rectangle */
static GLuint vaoCirc = 0, vboCirc = 0;  /* unit circle fan (center + ring) */
static GLsizei circleCount = 0;

static const char* VERT_SRC =
"#version 300 es\n"
"layout(location=0) in vec2 aPos;\n"
"uniform vec2 uResolution;\n"
"uniform vec2 uScale;\n"
"uniform vec2 uTranslate;\n"
"void main(){\n"
"  vec2 pos = aPos * uScale + uTranslate; /* pixels */\n"
"  vec2 ndc = (pos / uResolution * 2.0 - 1.0) * vec2(1.0, -1.0);\n"
"  gl_Position = vec4(ndc,0.0,1.0);\n"
"}\n";

static const char* FRAG_SRC =
"#version 300 es\n"
"precision mediump float;\n"
"uniform vec4 uColor;\n"
"out vec4 outColor;\n"
"void main(){ outColor = uColor; }\n";

static GLuint compile(GLenum type, const char* src){
  GLuint sh = glCreateShader(type);
  glShaderSource(sh, 1, &src, NULL);
  glCompileShader(sh);
#ifdef DEBUG
  GLint ok=0; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
  if(!ok){ char log[1024]; GLsizei n=0; glGetShaderInfoLog(sh,1024,&n,log); printf("shader:\n%.*s\n",n,log); }
#endif
  return sh;
}

static int makeProgram(){
  GLuint vs = compile(GL_VERTEX_SHADER, VERT_SRC);
  GLuint fs = compile(GL_FRAGMENT_SHADER, FRAG_SRC);
  prog = glCreateProgram();
  glAttachShader(prog, vs); glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs); glDeleteShader(fs);
#ifdef DEBUG
  GLint ok=0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if(!ok){ char log[1024]; GLsizei n=0; glGetProgramInfoLog(prog,1024,&n,log); printf("program:\n%.*s\n",n,log); return 0; }
#endif
  uResolution = glGetUniformLocation(prog, "uResolution");
  uScale      = glGetUniformLocation(prog, "uScale");
  uTranslate  = glGetUniformLocation(prog, "uTranslate");
  uColor      = glGetUniformLocation(prog, "uColor");
  return 1;
}

static void makeUnitRect(){
  if(vaoRect) return;
  const float verts[] = {
    -0.5f,-0.5f,  -0.5f, 0.5f,   0.5f, 0.5f,
    -0.5f,-0.5f,   0.5f, 0.5f,   0.5f,-0.5f
  };
  glGenVertexArrays(1,&vaoRect);
  glGenBuffers(1,&vboRect);
  glBindVertexArray(vaoRect);
  glBindBuffer(GL_ARRAY_BUFFER, vboRect);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
}

static void makeUnitCircle(int segments){
  if(vaoCirc) return;
  circleCount = segments + 2; /* center + segments+1 */
  float* v = (float*)malloc(sizeof(float)*2*circleCount);
  v[0]=0.0f; v[1]=0.0f; /* center */
  for(int i=0;i<=segments;++i){
    float t = (float)i/(float)segments;
    float th = 2.0f*(float)M_PI*t;
    v[(i+1)*2+0] = cosf(th)*0.5f;
    v[(i+1)*2+1] = sinf(th)*0.5f;
  }
  glGenVertexArrays(1,&vaoCirc);
  glGenBuffers(1,&vboCirc);
  glBindVertexArray(vaoCirc);
  glBindBuffer(GL_ARRAY_BUFFER, vboCirc);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float)*2*circleCount, v, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),(void*)0);
  free(v);
}

/* ------------------------------ UI (DOM) -------------------------------- */
static void ui_set_score(int s0, int s1){
  EM_ASM({
    var l = document.getElementById('scoreL');
    var r = document.getElementById('scoreR');
    if(l) l.textContent = ($0<10?"0":"")+$0;
    if(r) r.textContent = ($1<10?"0":"")+$1;
  }, s0, s1);
}
static void ui_set_msg(const char* msg){
  EM_ASM({ var el=document.getElementById('msg'); if(el) el.textContent=UTF8ToString($0); }, msg);
}
static void ui_set_mode_1p2p(int n){
  EM_ASM({ var el=document.getElementById('mode'); if(el) el.textContent = ($0==1?"1 Player":"2 Players"); }, n);
}
static void ui_set_title(const char* t){
  EM_ASM({ var el=document.getElementById('title'); if(el) el.textContent = UTF8ToString($0); }, t);
}
static void ui_set_score_colors(const float* cL, const float* cR){
  EM_ASM({
    function css(r,g,b){return `rgb(${Math.round(r*255)},${Math.round(g*255)},${Math.round(b*255)})`;}
    var l=document.getElementById('scoreL'); var r=document.getElementById('scoreR');
    if(l) l.style.color = css(HEAPF32[($0>>2)], HEAPF32[(($0>>2)+1)], HEAPF32[(($0>>2)+2)]);
    if(r) r.style.color = css(HEAPF32[($1>>2)], HEAPF32[(($1>>2)+1)], HEAPF32[(($1>>2)+2)]);
  }, cL, cR);
}

/* ------------------------------ WebAudio -------------------------------- */
EM_JS(void, js_audio_init, (), {
  if(!Module.audio) Module.audio = { ctx:null, ready:false, lists:{}, music:null, unlocked:false };
  if(!Module.audio.ctx){
    try{ Module.audio.ctx = new (window.AudioContext||window.webkitAudioContext)(); }
    catch(e){ console.error('AudioContext failed', e); }
  }
});

EM_JS(void, js_audio_resume, (), {
  var A=Module.audio; if(!A||!A.ctx) return;
  if(A.ctx.state!=="running"){ A.ctx.resume(); }
  A.unlocked = true;
});

EM_JS(void, js_audio_load_all, (), {
  var A=Module.audio; if(!A||!A.ctx) return;
  const multi = { hit:5, bounce:5 };
  const singles = ["hit_slow","hit_medium","hit_fast","hit_veryfast","bounce_synth","score_goal","up","down"];
  function decode(path){
    try{ var data = FS.readFile(path); return A.ctx.decodeAudioData(data.buffer.slice(0)); }
    catch(e){ return Promise.reject(e); }
  }
  var ps=[]; A.lists={};
  Object.keys(multi).forEach(function(k){ A.lists[k]=[]; for(var i=0;i<multi[k];i++){ let p=`/sounds/${k}${i}.ogg`; ps.push(decode(p).then(b=>A.lists[k].push(b)).catch(()=>{})); }});
  singles.forEach(function(k){ let p0=`/sounds/${k}0.ogg`, p1=`/sounds/${k}.ogg`; let pick=null;
    try{ FS.lookupPath(p0); pick=p0; }catch(_){} if(!pick){ try{ FS.lookupPath(p1); pick=p1; }catch(__){} }
    if(pick){ ps.push(decode(pick).then(b=>A.lists[k]=[b]).catch(()=>{})); }
  });
  Promise.all(ps).then(()=>{ A.ready=true; console.log('SFX loaded'); }).catch(()=>{ A.ready=true; });
});

EM_JS(void, js_audio_play, (const char* cname, int count), {
  var A=Module.audio; if(!A||!A.ctx||!A.ready||!A.unlocked) return;
  var name=UTF8ToString($0); var arr=A.lists[name]; if(!arr||!arr.length) return;
  for(var i=0;i<$1;i++){
    var buf = arr[(Math.random()*arr.length)|0];
    var src=A.ctx.createBufferSource(); src.buffer=buf; src.connect(A.ctx.destination); src.start();
  }
});

EM_JS(void, js_music_try_play, (), {
  var A=Module.audio; if(!A||!A.ctx||A.music||!A.unlocked) return;
  try{
    var data = FS.readFile('/music/theme.ogg');
    A.ctx.decodeAudioData(data.buffer.slice(0)).then(function(buf){
      var src=A.ctx.createBufferSource(); src.buffer=buf; src.loop=true;
      var gain=A.ctx.createGain(); gain.gain.value=0.3; src.connect(gain); gain.connect(A.ctx.destination);
      A.music=src; A.musicGain=gain; src.start();
    }).catch(()=>{});
  }catch(e){}
});

static void sfx_play(const char* name, int count){ js_audio_play(name, count); }

/* ---------------------------- Game Structures --------------------------- */
typedef enum { ST_MENU=1, ST_PLAY=2, ST_OVER=3 } State;

typedef struct { float x,y; int score; int timer; int isAI; } Bat;
typedef struct { float x,y, dx,dy; int speed; float prev_x; } Ball;
typedef struct { float x,y; int time; } Impact;

#define MAX_IMPACTS 64

typedef struct {
  Bat  bats[2];
  Ball ball;
  Impact impacts[MAX_IMPACTS]; int nImpacts;
  int  numPlayers; /* 1 or 2 */
  int  ai_offset;  /* -10..10 */
  int  music_started;
  State state;
} Game;

static Game G;

/* ----------------------------- Input State ------------------------------ */
static int key_a=0,key_z=0,key_up=0,key_down=0,key_k=0,key_m=0;
static int space_down=0; /* latched on keydown */

static EM_BOOL on_key(int type, const EmscriptenKeyboardEvent* e, void* user){
  int down = (type==EMSCRIPTEN_EVENT_KEYDOWN);
  if(!strcmp(e->key, "ArrowUp"))   { key_up   = down; return EM_TRUE; }
  if(!strcmp(e->key, "ArrowDown")) { key_down = down; return EM_TRUE; }
  if(!strcmp(e->key, "a") || !strcmp(e->key, "A")) { key_a = down; return EM_TRUE; }
  if(!strcmp(e->key, "z") || !strcmp(e->key, "Z")) { key_z = down; return EM_TRUE; }
  if(!strcmp(e->key, "k") || !strcmp(e->key, "K")) { key_k = down; return EM_TRUE; }
  if(!strcmp(e->key, "m") || !strcmp(e->key, "M")) { key_m = down; return EM_TRUE; }
  if(!strcmp(e->key, " ") || !strcmp(e->code,"Space") || !strcmp(e->key,"Spacebar")) { if(down) space_down=1; return EM_TRUE; }
  return EM_FALSE;
}

/* ------------------------------ Math Utils ------------------------------ */
static void normalised(float* x, float* y){
  float len = sqrtf((*x)*(*x) + (*y)*(*y));
  if(len<=0.0f){ *x=0; *y=0; return; }
  *x /= len; *y /= len;
}

/* ----------------------------- Game Helpers ----------------------------- */
static void impact_add(float x,float y){
  if(G.nImpacts<MAX_IMPACTS){ G.impacts[G.nImpacts].x=x; G.impacts[G.nImpacts].y=y; G.impacts[G.nImpacts].time=0; G.nImpacts++; }
}
static void impacts_update(){
  int w=0; for(int i=0;i<G.nImpacts;i++){ Impact im=G.impacts[i]; im.time++; if(im.time<10){ G.impacts[w++]=im; } }
  G.nImpacts=w;
}

static void reset_ball_toward(int loser){
  G.ball.x = WIDTH/2.0f; G.ball.y = HEIGHT/2.0f;
  G.ball.dx = (loser==0? -1.0f : 1.0f);
  G.ball.dy = 0.0f; G.ball.speed = 5; G.ball.prev_x = G.ball.x;
}

static void new_game(){
  G.bats[0].x = 40;        G.bats[0].y = HEIGHT/2.0f; G.bats[0].score=0; G.bats[0].timer=0; G.bats[0].isAI = 0;
  G.bats[1].x = WIDTH-40;  G.bats[1].y = HEIGHT/2.0f; G.bats[1].score=0; G.bats[1].timer=0; G.bats[1].isAI = (G.numPlayers==1);
  G.ai_offset = 0; G.nImpacts=0; reset_ball_toward(1);
  ui_set_score(0,0);
}

static float p1_controls(){
  if(key_z || key_down) return  PLAYER_SPEED;
  if(key_a || key_up)   return -PLAYER_SPEED;
  return 0.0f;
}
static float p2_controls(){
  if(key_m) return  PLAYER_SPEED;
  if(key_k) return -PLAYER_SPEED;
  return 0.0f;
}
static float ai_control(int right){
  float xdist = fabsf(G.ball.x - (right? G.bats[1].x : G.bats[0].x));
  float t1 = HEIGHT/2.0f;
  float t2 = G.ball.y + (float)G.ai_offset;
  float w1 = fmaxf(0.0f, fminf(1.0f, xdist / (WIDTH/2.0f)));
  float target = w1*t1 + (1.0f - w1)*t2;
  float delta = target - (right? G.bats[1].y : G.bats[0].y);
  if(delta >  MAX_AI_SPEED) delta =  MAX_AI_SPEED;
  if(delta < -MAX_AI_SPEED) delta = -MAX_AI_SPEED;
  return delta;
}

static void update_game(){
  /* paddles */
  float dy0 = p1_controls();
  float dy1 = (G.numPlayers==2? p2_controls() : ai_control(1));
  G.bats[0].y += dy0; if(G.bats[0].y<80) G.bats[0].y=80; if(G.bats[0].y>400) G.bats[0].y=400;
  G.bats[1].y += dy1; if(G.bats[1].y<80) G.bats[1].y=80; if(G.bats[1].y>400) G.bats[1].y=400;
  G.bats[0].timer--; G.bats[1].timer--;

  /* ball: integrate in 'speed' microsteps to emulate pygame cadence */
  int steps = G.ball.speed;
  for(int s=0;s<steps;++s){
    G.ball.prev_x = G.ball.x;
    G.ball.x += G.ball.dx;
    G.ball.y += G.ball.dy;

    const float halfW = 9.0f;   /* paddle half width */
    const float halfH = 64.0f;  /* paddle half height */
    const float R     = 7.0f;   /* ball radius */

    /* left paddle */
    if(G.ball.x - R <= G.bats[0].x + halfW && G.ball.prev_x - R > G.bats[0].x + halfW){
      float diff_y = G.ball.y - G.bats[0].y;
      if(diff_y>-halfH && diff_y<halfH){
        G.ball.dx = -G.ball.dx;
        G.ball.dy += diff_y / 128.0f; if(G.ball.dy>1.0f) G.ball.dy=1.0f; if(G.ball.dy<-1.0f) G.ball.dy=-1.0f;
        normalised(&G.ball.dx,&G.ball.dy);
        G.ball.x = G.bats[0].x + halfW + R;
        G.ball.speed++;
        G.ai_offset = (rand()%21)-10;
        G.bats[0].timer = 10;
        impact_add(G.ball.x - G.ball.dx*10.0f, G.ball.y);
        sfx_play("hit",5);
        if(G.ball.speed<=10) sfx_play("hit_slow",1); else if(G.ball.speed<=12) sfx_play("hit_medium",1); else if(G.ball.speed<=16) sfx_play("hit_fast",1); else sfx_play("hit_veryfast",1);
      }
    }
    /* right paddle */
    if(G.ball.x + R >= G.bats[1].x - halfW && G.ball.prev_x + R < G.bats[1].x - halfW){
      float diff_y = G.ball.y - G.bats[1].y;
      if(diff_y>-halfH && diff_y<halfH){
        G.ball.dx = -G.ball.dx;
        G.ball.dy += diff_y / 128.0f; if(G.ball.dy>1.0f) G.ball.dy=1.0f; if(G.ball.dy<-1.0f) G.ball.dy=-1.0f;
        normalised(&G.ball.dx,&G.ball.dy);
        G.ball.x = G.bats[1].x - halfW - R;
        G.ball.speed++;
        G.ai_offset = (rand()%21)-10;
        G.bats[1].timer = 10;
        impact_add(G.ball.x - G.ball.dx*10.0f, G.ball.y);
        sfx_play("hit",5);
        if(G.ball.speed<=10) sfx_play("hit_slow",1); else if(G.ball.speed<=12) sfx_play("hit_medium",1); else if(G.ball.speed<=16) sfx_play("hit_fast",1); else sfx_play("hit_veryfast",1);
      }
    }

    /* walls */
    if(G.ball.y - R <= 0){ G.ball.dy = fabsf(G.ball.dy); G.ball.y = R; impact_add(G.ball.x, G.ball.y); sfx_play("bounce",5); sfx_play("bounce_synth",1); }
    if(G.ball.y + R >= HEIGHT){ G.ball.dy = -fabsf(G.ball.dy); G.ball.y = HEIGHT - R; impact_add(G.ball.x, G.ball.y); sfx_play("bounce",5); sfx_play("bounce_synth",1); }
  }

  impacts_update();

  /* scoring */
  int out_left  = (G.ball.x + 7.0f) < 0.0f;
  int out_right = (G.ball.x - 7.0f) > (float)WIDTH;
  if(out_left || out_right){
    int scorer = out_left ? 1 : 0;
    int loser  = 1 - scorer;
    if(G.bats[loser].timer < 0){
      G.bats[scorer].score += 1; ui_set_score(G.bats[0].score, G.bats[1].score);
      G.bats[loser].timer = 20; sfx_play("score_goal",1);
    } else if(G.bats[loser].timer == 0){
      reset_ball_toward(loser);
    }
  }

  /* win */
  if(G.bats[0].score>=10 || G.bats[1].score>=10){
    G.state = ST_OVER; ui_set_msg("Game Over — SPACE to return to menu");
  }
}

/* ------------------------------ Rendering ------------------------------- */
static void setColor(const float c[4]){ glUniform4f(uColor, c[0],c[1],c[2],c[3]); }
static void drawRect(float cx,float cy,float w,float h,const float col[4]){
  glBindVertexArray(vaoRect);
  glUniform2f(uScale, w, h);
  glUniform2f(uTranslate, cx, cy);
  setColor(col);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}
static void drawCircleFilled(float cx,float cy,float r,const float col[4]){
  glBindVertexArray(vaoCirc);
  glUniform2f(uScale, 2*r, 2*r);
  glUniform2f(uTranslate, cx, cy);
  setColor(col);
  glDrawArrays(GL_TRIANGLE_FAN, 0, circleCount);
}
static void drawCircleOutline(float cx,float cy,float r,const float col[4]){
  glBindVertexArray(vaoCirc);
  glUniform2f(uScale, 2*r, 2*r);
  glUniform2f(uTranslate, cx, cy);
  setColor(col);
  /* WebGL2 line width is effectively 1; this is fine for the ripple */
  glDrawArrays(GL_LINE_LOOP, 1, circleCount-1); /* skip center */
}

static void draw_center_line(){
  for(int y=0;y<HEIGHT;y+=20){
    drawRect(WIDTH/2.0f, (float)y+5.0f, 4.0f, 10.0f, WHITE);
  }
}

static void draw_impacts(){
  for(int i=0;i<G.nImpacts;i++){
    float r = 2.0f + G.impacts[i].time * 1.5f;
    float a = fmaxf(0.0f, 1.0f - G.impacts[i].time * 0.1f);
    float col[4] = {1.0f,1.0f,1.0f,a};
    drawCircleOutline(G.impacts[i].x, G.impacts[i].y, r, col);
  }
}

static void render(){
  glClearColor(GREEN[0],GREEN[1],GREEN[2],1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(prog);
  glUniform2f(uResolution, (float)WIDTH, (float)HEIGHT);

  draw_center_line();
  draw_impacts();

  const float *col0 = (G.bats[0].timer>0 && (G.ball.x<0||G.ball.x>WIDTH)) ? RED : WHITE;
  const float *col1 = (G.bats[1].timer>0 && (G.ball.x<0||G.ball.x>WIDTH)) ? BLUE: WHITE;
  drawRect(G.bats[0].x, G.bats[0].y, 18.0f, 128.0f, col0);
  drawRect(G.bats[1].x, G.bats[1].y, 18.0f, 128.0f, col1);

  drawCircleFilled(G.ball.x, G.ball.y, 7.0f, WHITE);

  /* tint HUD scores similar to pygame behavior */
  const float *scL = (G.bats[1].timer>0 && (G.ball.x<0||G.ball.x>WIDTH)) ? RED : WHITE;
  const float *scR = (G.bats[0].timer>0 && (G.ball.x<0||G.ball.x>WIDTH)) ? BLUE: WHITE;
  ui_set_score_colors(scL, scR);
}

/* --------------------------- Main Loop / State --------------------------- */
static void tick(void){
  if(G.state==ST_MENU){
    static int last_up=0, last_down=0;
    if(key_up && !last_up){ G.numPlayers=1; ui_set_mode_1p2p(1); sfx_play("up",1); }
    if(key_down && !last_down){ G.numPlayers=2; ui_set_mode_1p2p(2); sfx_play("down",1); }
    last_up=key_up; last_down=key_down;

    if(space_down){
      space_down=0;
      js_audio_resume(); if(!G.music_started){ js_music_try_play(); G.music_started=1; }
      G.state = ST_PLAY; new_game(); G.bats[1].isAI = (G.numPlayers==1); ui_set_msg("");
    }
  }
  else if(G.state==ST_PLAY){
    update_game();
  }
  else if(G.state==ST_OVER){
    if(space_down){
      space_down=0; G.state = ST_MENU; G.numPlayers=1; ui_set_mode_1p2p(1);
      ui_set_msg("UP/DOWN to select 1P/2P — SPACE to start");
    }
  }

  render();
}

/* ------------------------------- Exports -------------------------------- */
EMSCRIPTEN_KEEPALIVE
int initWebGL(void){
  static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = 0;
  if(!ctx){
    EmscriptenWebGLContextAttributes attr; emscripten_webgl_init_context_attributes(&attr);
    attr.majorVersion=2; attr.minorVersion=0; attr.depth=EM_FALSE;
    ctx = emscripten_webgl_create_context("#canvas", &attr);
    if(ctx<=0){ printf("webgl2 context failed\n"); return 0; }
    emscripten_webgl_make_context_current(ctx);
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 1, on_key);
    emscripten_set_keyup_callback  (EMSCRIPTEN_EVENT_TARGET_DOCUMENT, 0, 1, on_key);

    int w,h; emscripten_get_canvas_element_size("#canvas", &w,&h);
    if(w!=WIDTH || h!=HEIGHT) emscripten_set_canvas_element_size("#canvas", WIDTH, HEIGHT);
    glViewport(0,0,WIDTH,HEIGHT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  }

  if(!makeProgram()) return 0;
  makeUnitRect();
  makeUnitCircle(64);

  G.state = ST_MENU; G.numPlayers=1; ui_set_mode_1p2p(1); G.music_started=0; G.nImpacts=0;
  ui_set_score(0,0);
  ui_set_title("Pong!");
  ui_set_msg("UP/DOWN to select 1P/2P — SPACE to start");

  js_audio_init();
  js_audio_load_all();

  return 1;
}

EMSCRIPTEN_KEEPALIVE
void startMainLoop(void){ emscripten_set_main_loop(tick, 0, 1); }
