#ifndef VOUCHER_OBJECT_H
#define VOUCHER_OBJECT_H

#include "sprite.h"
#include "voucher.h"

#define VOUCHER_OBJECT_WIDTH  32
#define VOUCHER_OBJECT_HEIGHT 32

typedef struct
{
    enum VoucherId id;
    bool focused;
    SpriteObject* sprite_object;
} VoucherObject;

void voucher_object_init(void);
VoucherObject* voucher_object_new(enum VoucherId id);
void voucher_object_destroy(VoucherObject** object);
void voucher_object_set_focus(VoucherObject* object, bool focus);
void voucher_object_set_description_scale(VoucherObject* object, bool description_scale);

#endif // VOUCHER_OBJECT_H
