#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <math.h>
#include <SDL2/SDL.h>

typedef struct
{
    float x, y, z;
}
Vertex;

typedef struct
{
    Vertex* vertex;
    int count;
    int max;
}
Vertices;

typedef struct
{
    int va, vb, vc;
}
Face;

typedef struct
{
    Face* face;
    int count;
    int max;
}
Faces;

typedef struct
{
    Vertex a, b, c;
}
Triangle;

typedef struct
{
    Triangle* triangle;
    int count;
}
Triangles;

typedef struct
{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* canvas;
    int xres;
    int yres;
}
Sdl;

typedef struct
{
    int x0, y0, x1, y1;
}
Box;

typedef struct
{
    float xt;
    float yt;
    float sens;
    const uint8_t* key;
}
Input;

static int ulns(FILE* const file)
{
    int ch = EOF;
    int lines = 0;
    int pc = '\n';
    while((ch = getc(file)) != EOF)
    {
        if(ch == '\n') lines++;
        pc = ch;
    }
    if(pc != '\n') lines++;
    rewind(file);
    return lines;
}

static char* ureadln(FILE* const file)
{
    int ch = EOF;
    int reads = 0;
    int size = 128;
    char* line = (char*) malloc(sizeof(char) * size);
    while((ch = getc(file)) != '\n' && ch != EOF)
    {
        line[reads++] = ch;
        if(reads + 1 == size)
            line = (char*) realloc(line, sizeof(char) * (size *= 2));
    }
    line[reads] = '\0';
    return line;
}

static Vertices vsnew(const int max)
{
    const Vertices vs = { (Vertex*) malloc(sizeof(Vertex) * max), 0, max };
    return vs;
}

static Vertices vsload(FILE* const file)
{
    Vertices vs = vsnew(128);
    const int lines = ulns(file);
    for(int i = 0; i < lines; i++)
    {
        char* line = ureadln(file);
        if(line[0] == 'v' && line[1] != 't' && line[1] != 'n')
        {
            if(vs.count == vs.max)
                vs.vertex = (Vertex*) realloc(vs.vertex, sizeof(Vertex) * (vs.max *= 2));
            Vertex v;
            sscanf(line, "v %f %f %f", &v.x, &v.y, &v.z);
            vs.vertex[vs.count++] = v;
        }
        free(line);
    }
    rewind(file);
    return vs;
}

static Faces fsnew(const int max)
{
    const Faces fs = { (Face*) malloc(sizeof(Face) * max), 0, max };
    return fs;
}

static Faces fsload(FILE* const file)
{
    Faces fs = fsnew(128);
    const int lines = ulns(file);
    for(int i = 0; i < lines; i++)
    {
        char* line = ureadln(file);
        if(line[0] == 'f')
        {
            if(fs.count == fs.max)
                fs.face = (Face*) realloc(fs.face, sizeof(Face) * (fs.max *= 2));
            int waste;
            Face f;
            sscanf(
                line,
                "f %d/%d/%d %d/%d/%d %d/%d/%d",
                &f.va, &waste, &waste,
                &f.vb, &waste, &waste,
                &f.vc, &waste, &waste);
            const Face indexed = { f.va - 1, f.vb - 1, f.vc - 1 };
            fs.face[fs.count++] = indexed;
        }
        free(line);
    }
    rewind(file);
    return fs;
}

static Triangles tsgen(const Vertices vs, const Faces fs)
{
    const Triangles ts = { (Triangle*) malloc(sizeof(Triangle) * fs.count), fs.count };
    for(int i = 0; i < fs.count; i++)
    {
        const Triangle t = {
            vs.vertex[fs.face[i].va],
            vs.vertex[fs.face[i].vb],
            vs.vertex[fs.face[i].vc],
        };
        ts.triangle[i] = t;
    }
    return ts;
}

static void spresent(const Sdl sdl)
{
    SDL_RenderPresent(sdl.renderer);
}

static void schurn(const Sdl sdl)
{
    const SDL_Rect dst = {
        (sdl.xres - sdl.yres) / 2,
        (sdl.yres - sdl.xres) / 2,
        sdl.yres, sdl.xres
    };
    SDL_RenderCopyEx(sdl.renderer, sdl.canvas, NULL, &dst, -90, NULL, SDL_FLIP_NONE);
}

static Sdl ssetup(const int xres, const int yres)
{
    Sdl sdl;
    SDL_Init(SDL_INIT_VIDEO);
    sdl.window = SDL_CreateWindow("water", 0, 0, xres, yres, SDL_WINDOW_SHOWN);
    sdl.renderer = SDL_CreateRenderer(sdl.window, -1, SDL_RENDERER_ACCELERATED);
    // To improve CPU line drawing cache speed the xres and yres for the painting canvas is reversed.
    // This offsets the canvas by 90 degrees. When the finished canvas frame is presented it will be
    // quickly rotated 90 degrees by the GPU. See: schurn(Sdl)
    sdl.canvas = SDL_CreateTexture(sdl.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, yres, xres);
    sdl.xres = xres;
    sdl.yres = yres;
    return sdl;
}

static void sunlock(const Sdl sdl)
{
    SDL_UnlockTexture(sdl.canvas);
}

static uint32_t* slock(const Sdl sdl)
{
    void* pixel;
    int pitch;
    SDL_LockTexture(sdl.canvas, NULL, &pixel, &pitch);
    return (uint32_t*) pixel;
}

static Triangle tviewport(const Triangle t, const Sdl sdl)
{
    const float w = sdl.xres / 2.0;
    const float h = sdl.yres / 2.0;
    const float z = 1.0 / 2.0;
    const Triangle v = {
        { w * (t.a.x + 1.0), h * (t.a.y + 1.0), z * (t.a.z + 1.0) },
        { w * (t.b.x + 1.0), h * (t.b.y + 1.0), z * (t.b.z + 1.0) },
        { w * (t.c.x + 1.0), h * (t.c.y + 1.0), z * (t.c.z + 1.0) },
    };
    return v;
}

static Triangle tperspective(const Triangle t)
{
    const float c = 3.0;
    const float za = 1.0 - t.a.z / c;
    const float zb = 1.0 - t.b.z / c;
    const float zc = 1.0 - t.c.z / c;
    const Triangle p = {
        { t.a.x / za, t.a.y / za, t.a.z },
        { t.b.x / zb, t.b.y / zb, t.b.z },
        { t.c.x / zc, t.c.y / zc, t.c.z },
    };
    return p;
}

// Vector subtraction.
static Vertex vs(const Vertex a, const Vertex b)
{
    const Vertex v = { a.x - b.x, a.y - b.y, a.z - b.z };
    return v;
}

// Vector cross product.
static Vertex vc(const Vertex a, const Vertex b)
{
    const Vertex c = { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    return c;
}

// Vector scalar multiplication.
static Vertex vm(const Vertex v, const float n)
{
    const Vertex m = { v.x * n, v.y * n, v.z * n };
    return m;
}

// Vector dot product.
static float vd(const Vertex a, const Vertex b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Vector length.
static float vl(const Vertex v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

// Vector unit.
static Vertex vu(const Vertex v)
{
    return vm(v, 1.0 / vl(v));
}

// Triangle barycentric coordinates.
static Vertex tbc(const Triangle t, const int x, const int y)
{
    const Vertex p = { (float) x, (float) y, 0.0 };
    const Vertex v0 = vs(t.b, t.a);
    const Vertex v1 = vs(t.c, t.a);
    const Vertex v2 = vs(p, t.a);
    const float d00 = vd(v0, v0);
    const float d01 = vd(v0, v1);
    const float d11 = vd(v1, v1);
    const float d20 = vd(v2, v0);
    const float d21 = vd(v2, v1);
    const float v = (d11 * d20 - d01 * d21) / (d00 * d11 - d01 * d01);
    const float w = (d00 * d21 - d01 * d20) / (d00 * d11 - d01 * d01);
    const float u = 1.0 - v - w;
    const Vertex vertex = { v, w, u };
    return vertex;
}

// Triangle normal.
static Vertex tnm(const Triangle t)
{
    return vc(vs(t.b, t.a), vs(t.c, t.a));
}

static float zget(const Triangle t, const Vertex bc)
{
    return bc.x * t.a.z + bc.y * t.b.z + bc.z * t.c.z;
}

static int vinside(const Vertex bc)
{
    return bc.x >= 0.0 && bc.y >= 0.0 && bc.z >= 0.0;
}

static void tdraw(const int yres, uint32_t* const pixel, const Triangle t, float* const zbuff, const int shade)
{
    const Box b = {
        (int) fminf(t.a.x, fminf(t.b.x, t.c.x)),
        (int) fminf(t.a.y, fminf(t.b.y, t.c.y)),
        (int) fmaxf(t.a.x, fmaxf(t.b.x, t.c.x)),
        (int) fmaxf(t.a.y, fmaxf(t.b.y, t.c.y)),
    };
    for(int x = b.x0; x <= b.x1; x++)
    for(int y = b.y0; y <= b.y1; y++)
    {
        const Vertex bc = tbc(t, x, y);
        if(vinside(bc))
        {
            const float z = zget(t, bc);
            // Remember that the canvas is rotated 90 degrees so the x and y are flipped here.
            if(z > zbuff[y + x * yres])
            {
                zbuff[y + x * yres] = z;
                pixel[y + x * yres] = shade;
            }
        }
    }
}

static Triangle tview(const Triangle t, const Vertex eye, const Vertex center, const Vertex up)
{
    const Vertex z = vu(vs(eye, center));
    const Vertex x = vu(vc(up, z));
    const Vertex y = vc(z, x);
    const float xe = vd(x, eye);
    const float ye = vd(y, eye);
    const float ze = vd(z, eye);
    const Triangle l = {
        { t.a.x * x.x + t.a.y * x.y + t.a.z * x.z - xe, t.a.x * y.x + t.a.y * y.y + t.a.z * y.z - ye, t.a.x * z.x + t.a.y * z.y + t.a.z * z.z - ze },
        { t.b.x * x.x + t.b.y * x.y + t.b.z * x.z - xe, t.b.x * y.x + t.b.y * y.y + t.b.z * y.z - ye, t.b.x * z.x + t.b.y * z.y + t.b.z * z.z - ze },
        { t.c.x * x.x + t.c.y * x.y + t.c.z * x.z - xe, t.c.x * y.x + t.c.y * y.y + t.c.z * y.z - ye, t.c.x * z.x + t.c.y * z.y + t.c.z * z.z - ze },
    };
    return l;
}

static Input iinit()
{
    const Input input = { 0.0, 0.0, 0.001, SDL_GetKeyboardState(NULL) };
    SDL_SetRelativeMouseMode(SDL_FALSE);
    return input;
}

static Input ipump(Input input)
{
    int dx, dy;
    SDL_PumpEvents();
    SDL_GetRelativeMouseState(&dx, &dy);
    input.xt -= input.sens * dx;
    input.yt += input.sens * dy;
    return input;
}

static Vertex ieye(const Input input)
{
    const Vertex e = { sinf(input.xt), sinf(input.yt), cosf(input.xt) };
    return e;
}

static void reset(float* const zbuff, uint32_t* const pixel, const int size)
{
    for(int i = 0; i < size; i++)
        zbuff[i] = -FLT_MAX;
    for(int i = 0; i < size; i++)
        pixel[i] = 0x0;
}

int main()
{
    const int xres = 800;
    const int yres = 600;
    const char* path = "obj/african_head.obj";
    FILE* const file = fopen(path, "r");
    if(!file)
        printf("could not open %s\n", path);
    const Vertices vs = vsload(file);
    const Faces fs = fsload(file);
    const Triangles ts = tsgen(vs, fs);
    const Sdl sdl = ssetup(xres, yres);
    const Vertex lights = { 0.0, 0.0, 1.0 };
    const Vertex center = { 0.0, 0.0, 0.0 };
    const Vertex upward = { 0.0, 1.0, 0.0 };
    const int size = xres * yres;
    float* const zbuff = (float*) malloc(sizeof(float) * size);
    for(Input input = iinit(); !input.key[SDL_SCANCODE_END]; input = ipump(input))
    {
        uint32_t* const pixel = slock(sdl);
        reset(zbuff, pixel, size);
        const Vertex eye = ieye(input);
        for(int i = 0; i < ts.count; i++)
        {
            const Triangle t = ts.triangle[i];
            const Triangle m = tview(t, eye, center, upward);
            const Triangle p = tperspective(m);
            const Triangle v = tviewport(p, sdl);
            const float brightness = vd(vu(tnm(p)), lights);
            const float shade = 0x0000FF * brightness;
            // Back face culling will not render the hidden triangles.
            if(brightness > 0.0)
                tdraw(yres, pixel, v, zbuff, shade);
        }
        sunlock(sdl);
        schurn(sdl);
        spresent(sdl);
    }
    // Theres no need to cleanup - the OS will do so and give us a quit exit.
}
