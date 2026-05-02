#pragma once
#include "widget.h"

typedef void* RenderHandle;

typedef struct Screen Screen;
struct Screen {
    void         (*create)(Screen *self);
    void         (*destroy)(Screen *self);
    void         (*on_show)(Screen *self);
    void         (*on_hide)(Screen *self);
    RenderHandle   root;
    void          *ctx;
};
