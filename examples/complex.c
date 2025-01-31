#include "example.h"

#include "cbor/decoder.h"
#include "cbor/parser.h"
#include "cbor/helper.h"

static void do_unknown(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	struct udt *p = (struct udt *)udt;
	if (item->type == CBOR_ITEM_SIMPLE_VALUE) {
		cbor_decode(reader, item, &p->type, sizeof(p->type));
	} else if (item->type == CBOR_ITEM_STRING && item->size == 16) {
		cbor_decode(reader, item, &p->uuid, sizeof(p->uuid));
	}
}
static void do_timestamp(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	struct udt *p = (struct udt *)udt;
	cbor_decode(reader, item, &p->time, sizeof(p->time));
}
static void do_acc_x(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	struct udt *p = (struct udt *)udt;
	cbor_decode(reader, item, &(p->data.acc.x), sizeof(p->data.acc.x));
}
static void do_acc_y(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	struct udt *p = (struct udt *)udt;
	cbor_decode(reader, item, &(p->data.acc.y), sizeof(p->data.acc.y));
}
static void do_acc_z(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	struct udt *p = (struct udt *)udt;
	cbor_decode(reader, item, &(p->data.acc.z), sizeof(p->data.acc.z));
}
static void do_gyr_x(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	struct udt *p = (struct udt *)udt;
	cbor_decode(reader, item, &(p->data.gyro.x), sizeof(p->data.gyro.x));
}
static void do_gyr_y(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	struct udt *p = (struct udt *)udt;
	cbor_decode(reader, item, &(p->data.gyro.y), sizeof(p->data.gyro.y));
}
static void do_gyr_z(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	struct udt *p = (struct udt *)udt;
	cbor_decode(reader, item, &(p->data.gyro.z), sizeof(p->data.gyro.z));
}

static struct converter {
	void const *primary;
	void const *secondary;
	void (*run)(cbor_reader_t const *reader, cbor_item_t const *item, void *udt);
} converters[] = {
	{ .primary = "t",   .secondary = NULL,	    .run = do_timestamp },
	{ .primary = "x",   .secondary = "acc",	    .run = do_acc_x },
	{ .primary = "y",   .secondary = "acc",	    .run = do_acc_y },
	{ .primary = "z",   .secondary = "acc",	    .run = do_acc_z },
	{ .primary = "x",   .secondary = "gyro",    .run = do_gyr_x },
	{ .primary = "y",   .secondary = "gyro",    .run = do_gyr_y },
	{ .primary = "z",   .secondary = "gyro",    .run = do_gyr_z },
};

static void (*get_converter(void const *primary, size_t primary_len,
			    void const *secondary, size_t secondary_len))
		(cbor_reader_t const *reader, cbor_item_t const *item, void *udt)
{
	if ((primary == NULL && secondary == NULL) ||
		(primary_len == 0 && secondary_len == 0)) {
		return do_unknown;
	}

	for (size_t i = 0; i < sizeof(converters) / sizeof(converters[0]); i++) {
		struct converter const *p = &converters[i];
		if (p->primary && memcmp(p->primary, primary, primary_len)) {
			continue;
		} else if (p->secondary && memcmp(p->secondary, secondary, secondary_len)) {
			continue;
		}

		if (p->primary || p->secondary) {
			return p->run;
		}
	}

	return do_unknown;
}

static void convert(cbor_reader_t const *reader, cbor_item_t const *item,
		    cbor_item_t const *parent, void *udt)
{
	void const *primary = NULL;
	void const *secondary = NULL;
	size_t primary_len = 0;
	size_t secondary_len = 0;

	if (parent != NULL && parent->type == CBOR_ITEM_MAP) {
		if ((item - parent) % 2) { /* key */
			return;
		}

		if (parent->offset > 1) {
			secondary = cbor_decode_pointer(reader, parent-1);
			secondary_len = (parent-1)->size;
		}

		primary = cbor_decode_pointer(reader, item-1);
		primary_len = (item-1)->size;
	}

	get_converter(primary, primary_len, secondary, secondary_len)
		(reader, item, udt);
}

/*
   sample_data[] = { 0x9f, 0xe1, 0x50, 0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12,
	0xd3, 0xa4, 0x56, 0x42, 0x66, 0x14, 0x17, 0x40, 0x00, 0xbf,
	0x61, 0x74, 0x1a, 0x63, 0x04, 0x4b, 0xa5, 0x64, 0x64, 0x61,
	0x74, 0x61, 0xa2, 0x63, 0x61, 0x63, 0x63, 0xa3, 0x61, 0x78,
	0xf9, 0x00, 0x00, 0x61, 0x79, 0xf9, 0x00, 0x00, 0x61, 0x7a,
	0xfa, 0x41, 0x1c, 0xf5, 0xc3, 0x64, 0x67, 0x79, 0x72, 0x6f,
	0xa3, 0x61, 0x78, 0xfa, 0x40, 0xc9, 0x99, 0x9a, 0x61, 0x79,
	0xf9, 0x00, 0x00, 0x61, 0x7a, 0xf9, 0x00, 0x00, 0xff, 0xff
   };

   generated by:
	cbor_writer_init(&writer, writer_buffer, sizeof(writer_buffer));
	cbor_encode_array_indefinite(&writer);
	  cbor_encode_simple(&writer, 1);
	  cbor_encode_byte_string(&writer, uuid, sizeof(uuid));
	  cbor_encode_map_indefinite(&writer);
	    cbor_encode_text_string(&writer, "t");
	    cbor_encode_unsigned_integer(&writer, 1661225893);
	    cbor_encode_text_string(&writer, "data");
	      cbor_encode_map(&writer, 2);
	        cbor_encode_text_string(&writer, "acc");
	        cbor_encode_map(&writer, 3);
	          cbor_encode_text_string(&writer, "x");
	          cbor_encode_float(&writer, 0.f);
	          cbor_encode_text_string(&writer, "y");
	          cbor_encode_float(&writer, 0.f);
	          cbor_encode_text_string(&writer, "z");
	          cbor_encode_float(&writer, 9.81f);
	        cbor_encode_text_string(&writer, "gyro");
	          cbor_encode_map(&writer, 3);
	          cbor_encode_text_string(&writer, "x");
	          cbor_encode_float(&writer, 6.3f);
	          cbor_encode_text_string(&writer, "y");
	          cbor_encode_float(&writer, 0.f);
	          cbor_encode_text_string(&writer, "z");
	          cbor_encode_float(&writer, 0.f);
	  cbor_encode_break(&writer);
	cbor_encode_break(&writer);

   See the test case: tests/src/example_test.cpp
 */
void complex_example(void const *data, size_t datasize, void *udt)
{
	cbor_reader_t reader;
	cbor_item_t items[32];
	size_t n;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	cbor_error_t err = cbor_parse(&reader, data, datasize, &n);
	if (err == CBOR_SUCCESS || err == CBOR_BREAK) {
		cbor_iterate(&reader, items, n, 0, convert, udt);
	}
}
