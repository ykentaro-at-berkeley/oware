#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <algorithm>
#include <utility>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_thread.h>
using namespace std;

void repaint();
int keyInt();

class OwareGame;

class OwareState {
    vector<int> value;
    int scorea, scoreb;
    int nOwnSeeds(bool a);
    void clearOwnSeeds(bool a);

    public:
    OwareState() : value(12, 4), scorea(0), scoreb(0) {}
    OwareState(const vector<int>& v, int sca, int scb) : value(v), scorea(sca), scoreb(scb) {}
    int sow(int pos);
    int sow(bool a, int pos);
    int capture(int last);
    int capture(bool a, int last);
    int endGame();
    const vector<int>& getValue() const { return value; }
    int getValue(bool a, int pos) { return value[pos + (a ? 0 : 6)]; }
    const int& getScore(bool a) const { return a ? scorea : scoreb; }
};

class OwareGameIface {
    const OwareState& game;
    bool alice;
    public:
    OwareGameIface(const OwareState& g, bool a) : game(g), alice(a) {}
    bool getSide() const { return alice; }
    const OwareState& getState() const { return game; }
    // vector<int> getValue() const;
};

class OwarePlayer {
    protected:
    string name;
    OwareGameIface *iface;
    public:
    OwarePlayer(const string& s) : name(s), iface(0) {}
    virtual ~OwarePlayer() {}
    const string& id() const { return name; }
    void registerIface(OwareGameIface *g) { iface = g; }
    virtual int choose() = 0;
};

class OwareCUIPlayer : public OwarePlayer {
    public:
    OwareCUIPlayer(const string& s) : OwarePlayer(s) {}
    virtual int choose();
};

class OwareABPlayer : public OwarePlayer {
    public:
    OwareABPlayer(const string& s) : OwarePlayer(s) {}
    virtual int choose();
};

class OwareKeyPlayer : public OwarePlayer {
    public:
    OwareKeyPlayer(const string& s) : OwarePlayer(s) {}
    virtual int choose();
};

class OwareGame {
    static const int PIT_WIDTH = 81, PIT_HEIGHT = 78;
    static const int B = 83;
    static const int pos[][2];
    static const int pos2[][2];

    SDL_Surface *pit[16], *board;
    OwarePlayer& alice;
    OwarePlayer& bob;
    OwareGameIface ia, ib;
    OwareState st;

    pair<SDL_Surface *, SDL_Surface *> twoPits(int p);
    bool printResult();
    OwarePlayer& getPlayer(bool a) { return a ? alice : bob; }
    
    public:
    static const int WIDTH = 516, HEIGHT = 357;
    OwareGame(OwarePlayer& a, OwarePlayer& b);
    virtual ~OwareGame();
    void loop();
    void display();
};

const int OwareGame::pos2[][2] = {
    {60+B*2, 43},
    {63+B*3, 45},
    {68+B*2, 308},
    {68+B*3, 305},
};

const int OwareGame::pos[][2]  = { // Z
    {54+B*0, 219},
    {54+B*1, 219},
    {58+B*2, 215},
    {58+B*3, 214},
    {58+B*4, 214},
    {58+B*5, 212},
    {58+B*5, 131},
    {57+B*4, 130},
    {55+B*3, 129},
    {53+B*2, 131},
    {52+B*1, 133},
    {52+B*0, 135},
};

pair<char *, int> findResource(const char *name) {
    extern char *stream_ptrs[];
    extern const char *stream_names[];
    extern const int stream_sizes[];
    extern int stream_count;

    for (int i = 0; i < stream_count; ++i) {
	if (!strcmp(stream_names[i], name))
	    return make_pair(stream_ptrs[i], stream_sizes[i]);
    }
    return make_pair((char *) 0, 0);
}

SDL_Surface *loadImage(const char *name) {
    pair<char *, int> p = findResource(name);
    if (!p.first)
	return 0;
    SDL_Surface *loaded = IMG_Load_RW(SDL_RWFromMem(p.first, p.second), true),
	*opt = 0;
    if (loaded) {
	opt = SDL_DisplayFormatAlpha(loaded);
	SDL_FreeSurface(loaded);
    }
    return opt;
}

int blitAt(SDL_Surface *obj, int x, int y, SDL_Surface *target) {
    SDL_Rect dest;
    dest.x = x, dest.y = y;
    return SDL_BlitSurface(obj, 0, target, &dest);
}

OwareGame::OwareGame(OwarePlayer& a, OwarePlayer& b)
    : alice(a), bob(b), 
      ia(st, true), ib(st, false) {
    alice.registerIface(&ia);
    bob.registerIface(&ib);
    for (int i = 0; i < 16; ++i) {
	static char buf[256];
	snprintf(buf, sizeof buf, "Trou%02d.png", i);
	pit[i] = loadImage(buf);
    }
    board = loadImage("Awale.png");
}

OwareGame::~OwareGame() {
    for (int i = 0; i < 16; ++i)
	SDL_FreeSurface(pit[i]);
    SDL_FreeSurface(board);
}

pair<SDL_Surface *, SDL_Surface *> OwareGame::twoPits(int p) {
    return make_pair(pit[min(15, p/2)], pit[min(15, p - p/2)]);
}

void OwareGame::display() {
    SDL_Surface *scr = SDL_GetVideoSurface();
    SDL_BlitSurface(board, 0, scr, 0);
    for (int i = 0; i < 12; ++i)
	blitAt(pit[min(15, st.getValue()[i])],
	       pos[i][0] - PIT_WIDTH/2, pos[i][1] - PIT_HEIGHT/2,
	       scr);
    for (int i = 0; i < 2; ++i) {
	int p = st.getScore(i);
	pair<SDL_Surface *, SDL_Surface *> two = twoPits(p);
	blitAt(two.first,
	       pos2[2*i][0] - PIT_WIDTH/2, pos2[2*i][1] - PIT_HEIGHT/2,
	       scr);
	blitAt(two.second,
	       pos2[2*i+1][0] - PIT_WIDTH/2, pos2[2*i+1][1] - PIT_HEIGHT/2,
	       scr);
    }
    SDL_Flip(scr);
}

int OwareState::sow(int pos) {
    int seeds = value[pos];
    if (!seeds)
	return -1;
    value[pos] = 0;
    int p;
    for (p = pos; seeds; p = (p+1) % 12) {
	if (p == pos)
	    continue;
	++value[p];
	--seeds;
    }
    return (p-1+12) % 12;
}

int OwareState::sow(bool a, int pos) {
    assert(0 <= pos && pos < 6);
    return sow(pos + (a ? 0 : 6));
}

int OwareState::capture(int last) {
    int p = last, s = 0;
    if (value[p] != 2 && value[p] != 3)
	return 0;
    for (;;) {
	s += value[p];
	if (p % 6 == 0)
	    break;
	int np = (p - 1 + 12) % 12;
	if (value[np] != 2 && value[np] != 3)
	    break;
	p = np;
    }
    int unc = 0;
    for (int i = last+1; i % 6; ++i)
	unc += value[i];
    if (!unc && p % 6 == 0) // "grand slam"
	return 0;
    for (int i = p; i <= last; ++i)
	value[i] = 0;
    return s;
}

int OwareState::capture(bool a, int last) {
    int info = 0;
    if (a == (last/6) % 2) {
	int& sc = a ? scorea : scoreb;
	info = capture(last);
	sc += info;
    }
    return info;
}

int OwareState::nOwnSeeds(bool a) {
    int start = (!a) * 6, sum = 0;
    for (int i = 0; i < 6; ++i)
	sum += value[start + i];
    return sum;
}

void OwareState::clearOwnSeeds(bool a) {
    int start = (!a) * 6;
    for (int i = 0; i < 6; ++i)
	value[start + i] = 0;
}

int OwareState::endGame() {
    for (int i = 0; i < 2; ++i) {
	if (nOwnSeeds(i) == 0) {
	    int r = nOwnSeeds(!i);
	    clearOwnSeeds(!i);
	    ((int &) getScore(!i)) += r;
	    return r;
	}
    }
    return -1;
}

bool OwareGame::printResult() {
    int sc;
    for (int i = 0; i < 2; ++i) {
	sc = st.getScore(i);
	if (sc > 24) {
	    cout << getPlayer(i).id() << " has captured " << sc
		 << " seeds and won!" << endl;
	    return true;
	}
    }

    if (sc == 24 && st.getScore(false) == 24) {
	cout << "Each players has captured 24 seeds and the game ends in a draw." << endl;
	return true;
    }
    return false;
}

void OwareGame::loop() {
    display();
    bool a = true;
    for (;; a = !a) {
	OwarePlayer& p = getPlayer(a);
	OwarePlayer& o = getPlayer(!a);
	int pos, last;
	for (;;) {
	    pos = p.choose();
	    last = st.sow(a, pos);
	    if (last >= 0)
		break;
	    cout << "Rooms with no seeds cannot be chosen." << endl;
	}
	display();
	SDL_Delay(1000);
	int info = st.capture(a, last);
	int info0 = st.endGame();
	if (info == 0)
	    cout << p.id() << " captures no seeds." << endl;
	else
	    cout << p.id() << " captures " << info << " seeds." << endl;
	if (info0 >= 0)
	    cout << o.id() << " captures remaining " << info0
		 << " seeds." << endl;
	display();
	SDL_Delay(1500);
	if (printResult())
	    break;
    }
    SDL_Delay(2000);
}

int OwareCUIPlayer::choose() {
    int tmp;
    for (;;) {
	cout << id() << "> ";
	cin >> tmp;
	if (0 <= tmp && tmp < 6)
	    return tmp;
	cout << "Choose a room number in [0, 5]." << endl;
    }
}

int OwareKeyPlayer::choose() {
    int tmp;
    for (;;) {
	tmp = keyInt();
	cerr << tmp << endl;
	if (0 <= tmp && tmp < 6)
	    return tmp;
	cout << "Choose a room number in [0, 5]." << endl;
    }
}

pair<int, int> minimax(const OwareState& st, bool min, bool side, int rest,
		       bool origSide) {
    if (!rest)
	return make_pair(0, st.getScore(origSide));
    int optp = -1, optv = 1<<30;
    for (int i = 0; i < 6; ++i) {
	OwareState cp = st;
	int l = cp.sow(side, i);
	if (l >= 0) {
	    cp.capture(side, l);
	    pair<int, int> cld = minimax(cp, !min, !side, rest-1, origSide);
	    int v = cld.second;
	    if (!min)
		v = -v;
	    if (optv > v) {
		optv = v;
		optp = i;
	    }
	}
    }
    if (!min)
	optv = -optv;
    //    fprintf(stderr, "min = %d, side = %d, rest = %d, optp = %d, optv = %d\n", int(min), int(side), rest, optp, optv);
    if (optp >= 0)
	return make_pair(optp, optv);
    else {
	OwareState cp = st;
	cp.endGame();
	return make_pair(0, cp.getScore(origSide));
    }
}

int OwareABPlayer::choose() {
    int p = minimax(iface->getState(), false, iface->getSide(), 5, iface->getSide()).first;
    cout << id() << " chooses " << p << '.' << endl;
    return p;
}

void repaint() {
    SDL_Event evt;
    evt.type = SDL_USEREVENT;
    SDL_PushEvent(&evt);
}

SDL_sem *keySem = 0;
int key;
int keyInt() {
    assert(keySem);
    SDL_SemWait(keySem);
    return key;
}

int startGameLoop(OwareGame *g) {
    g->loop();
    SDL_Event evt;
    evt.type = SDL_USEREVENT+1;
    SDL_PushEvent(&evt);
    return 0;
}

int main() {
    SDL_Init(SDL_INIT_EVERYTHING);
    int bpp = SDL_GetVideoInfo()->vfmt->BitsPerPixel;
    SDL_SetVideoMode(OwareGame::WIDTH, OwareGame::HEIGHT, bpp, SDL_SWSURFACE);
    {
	OwareKeyPlayer alice("Sayaka");
	OwareABPlayer bob("Kyoko");
	OwareGame game(alice, bob);
	SDL_EnableUNICODE(true);
	keySem = SDL_CreateSemaphore(0);
	SDL_Thread *thread = SDL_CreateThread((int (*) (void *)) startGameLoop,
					      &game);
	for (;;) {
	    SDL_Event evt;
	    SDL_WaitEvent(&evt);
	    switch (evt.type) {
	    case SDL_VIDEOEXPOSE:
	    case SDL_USEREVENT:
		game.display();
		break;
	    case SDL_USEREVENT+1:
		goto end;
	    case SDL_KEYDOWN:
		key = char(evt.key.keysym.unicode & 0x7f) - '0';
		SDL_SemPost(keySem);
		break;
	    case SDL_QUIT:
		goto end;
	    }
	}
	end:
	SDL_KillThread(thread);
	SDL_DestroySemaphore(keySem);
    }
    SDL_Quit();
    return 0;
}

