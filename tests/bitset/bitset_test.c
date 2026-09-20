#include "bitset.h"
#include "util.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

BITSET_DEFINE(test_bitset, BITSET_MAX_BITS)
BITSET_DEFINE(partial_bitset, 35)

// bitset_set_idx
// bitset_get_idx
// bitset_num_set_bits
// bitset_is_empty
// bitset_clear
void test_bitset_fill_all_and_empty(void)
{
    assert(bitset_is_empty(&test_bitset));

    for(int i = 0; i < BITSET_MAX_BITS; i++)
    {
        bitset_set_idx(&test_bitset, i, true);
        assert(bitset_num_set_bits(&test_bitset) == (i + 1));
    }

    for(int i = 0; i < BITSET_MAX_BITS; i++)
    {
        assert(bitset_get_idx(&test_bitset, i));
    }

    assert(!bitset_is_empty(&test_bitset));

    bitset_clear(&test_bitset);

    for(int i = 0; i < BITSET_MAX_BITS; i++)
    {
        assert(!bitset_get_idx(&test_bitset, i));
    }

    assert(bitset_is_empty(&test_bitset));
}

// bitset_set_idx
// bitset_num_set_bits
// bitset_find_idx_of_nth_set
// bitset_is_empty
// bitset_clear
void test_bitset_insertions_at_boundry(void)
{
    assert(bitset_is_empty(&test_bitset));

    bitset_set_idx(&test_bitset, 30, true);
    bitset_set_idx(&test_bitset, 31, true);
    bitset_set_idx(&test_bitset, 32, true);

    assert(bitset_num_set_bits(&test_bitset) == 3);

    bitset_set_idx(&test_bitset, 31, false);

    assert(bitset_num_set_bits(&test_bitset) == 2);

    assert(bitset_find_idx_of_nth_set(&test_bitset, 0) == 30);
    assert(bitset_find_idx_of_nth_set(&test_bitset, 1) == 32);

    bitset_set_idx(&test_bitset, 0, true);

    assert(bitset_find_idx_of_nth_set(&test_bitset, 0) == 0);

    bitset_set_idx(&test_bitset, 0, false);
    bitset_set_idx(&test_bitset, 0, 30);

    bitset_clear(&test_bitset);

    assert(bitset_is_empty(&test_bitset));
}

// bitset_set_idx
// bitset_is_empty
// bitset_clear
// bitset_itr_create
// bitset_itr_next
void test_bitset_iterator(void)
{
    assert(bitset_is_empty(&test_bitset));

    int test_indices[6] = {0, 31, 32, 63, 64, 100};

    for(int i = 0; i < 6; i++)
    {
        bitset_set_idx(&test_bitset, test_indices[i], true);
    }

    BitsetItr itr = bitset_itr_create(&test_bitset);
    
    int test_val = UNDEFINED;
    int index = 0;
    while((test_val = bitset_itr_next(&itr)) != UNDEFINED)
    {
        assert(test_val == test_indices[index++]);
    }

    assert(index == 6);

    bitset_clear(&test_bitset);

    assert(bitset_is_empty(&test_bitset));
}

void test_partial_word_and_invalid_indices(void)
{
    bitset_clear(&partial_bitset);
    assert(bitset_set_next_free_idx(&partial_bitset) == 0);
    for (int i = 1; i < 35; i++)
        assert(bitset_set_next_free_idx(&partial_bitset) == i);
    assert(bitset_num_set_bits(&partial_bitset) == 35);
    assert(bitset_set_next_free_idx(&partial_bitset) == UNDEFINED);
    assert(bitset_num_set_bits(&partial_bitset) == 35);

    bitset_set_idx(&partial_bitset, -1, false);
    bitset_set_idx(&partial_bitset, 35, false);
    assert(!bitset_get_idx(&partial_bitset, -1));
    assert(!bitset_get_idx(&partial_bitset, 35));

    BitsetItr itr = bitset_itr_create(&partial_bitset);
    int count = 0;
    while (bitset_itr_next(&itr) != UNDEFINED)
        count++;
    assert(count == 35);
}

int main(void)
{
    printf("Testing Bitset Fill All and Empty.\n");
    test_bitset_fill_all_and_empty();
    printf("Testing Bitset Insertions At Boundry.\n");
    test_bitset_insertions_at_boundry();
    printf("Testing Bitset Iterator.\n");
    test_bitset_iterator();
    printf("Testing partial words and invalid indices.\n");
    test_partial_word_and_invalid_indices();

    printf("-------------------------------------------------------------------------------\n");
    printf("Bitset Tests Passed :)\n");
    printf("-------------------------------------------------------------------------------\n");

    return 0;
}
