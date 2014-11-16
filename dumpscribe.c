#define _GNU_SOURCE
#include <openobex/obex.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <glib.h>
#include <libusb.h>
#include <archive.h>
#include <archive_entry.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define MAX_PATH_LENGTH (65536)

#define LS_VENDOR_ID 0x1cfb //LiveScribe Vendor ID
inline int is_ls_pulse(unsigned int c) { return (c == 0x1020 || c == 0x1010); } // LiveScribe Pulse(TM) Smartpen
inline int is_ls_echo(unsigned int c) { return (c == 0x1030 || c == 0x1032); } // LiveScribe Echo(TM) Smartpen

struct obex_state {
    obex_t *handle;
    int req_done;
    char *body;
    uint32_t body_len;
    int got_connid;
    int connid;
};

int debug_mode = 0;

void debug(const char* format, ... ) {
  if(debug_mode) {
    va_list arglist;
    va_start( arglist, format );
    vprintf( format, arglist );
    va_end( arglist );
  }
}


// not sure why this is necessary
// it seems to be some sort of hackish device reset 
void swizzle_usb(short vendor, short product) {
    libusb_context* ctx;
    libusb_device_handle* dev;
    int rc;

    rc = libusb_init(&ctx);
    assert(rc == 0);
    dev = libusb_open_device_with_vid_pid(ctx, vendor, product);
    assert(dev);

    libusb_set_configuration(dev, 1);

    // toggle between interfaces on usb multifunction device?
    libusb_set_interface_alt_setting(dev, 1, 0);
    libusb_set_interface_alt_setting(dev, 1, 1);

    libusb_close(dev);
    libusb_exit(ctx);
}

void obex_requestdone(struct obex_state* state, obex_t* hdl,
                            obex_object_t* obj, int cmd, int resp) {
    uint8_t header_id;
    obex_headerdata_t hdata;
    uint32_t hlen;

    switch (cmd & ~OBEX_FINAL) {
        case OBEX_CMD_CONNECT:
            while (OBEX_ObjectGetNextHeader(hdl, obj, &header_id,
                                            &hdata, &hlen)) {
                if (header_id == OBEX_HDR_CONNECTION) {
                    state->got_connid=1;
                    state->connid = hdata.bq4;
                }
            }
            break;

        case OBEX_CMD_GET:
            while (OBEX_ObjectGetNextHeader(hdl, obj, &header_id,
                                            &hdata, &hlen)) {
                if (header_id == OBEX_HDR_BODY ||
                    header_id == OBEX_HDR_BODY_END) {
                    if (state->body)
                        free(state->body);
                    state->body = (char*)malloc(hlen);
                    state->body_len = hlen;
                    memcpy(state->body, hdata.bs, hlen);
                    break;
                }
            }
            break;
    }

    state->req_done++;
}

void obex_event(obex_t* hdl, obex_object_t* obj, int mode, int event, int obex_cmd, int obex_rsp) {
    struct obex_state* state;
    obex_headerdata_t hd;

    state = (struct obex_state*) OBEX_GetUserData(hdl);

    if (event == OBEX_EV_PROGRESS) {
        hd.bq4 = state->connid;
        const int size = 4;
        const unsigned int flags = 0;
        const int rc = OBEX_ObjectAddHeader(hdl, obj, OBEX_HDR_CONNECTION, hd, size, flags);
        if (rc < 0) {
          fprintf(stderr, "OBEX adding header failed\n");
          obex_requestdone(state, hdl, obj, obex_cmd, obex_rsp);
          return;
        }
    } else if (obex_rsp != OBEX_RSP_SUCCESS && obex_rsp != OBEX_RSP_CONTINUE) {
      if(obex_rsp == OBEX_RSP_NOT_FOUND) {
        fprintf(stderr, "OBEX object not found.\n");
      } else {
        fprintf(stderr, "Unrecognized OBEX event encountered: %d.\n", obex_rsp);
        fprintf(stderr, "See http://dev.zuckschwerdt.org/openobex/doxygen/html/obex__const_8h.html for more info\n");
      }
      obex_requestdone(state, hdl, obj, obex_cmd, obex_rsp);
      return;
    } else {
        switch (event) {
            case OBEX_EV_REQDONE:
                obex_requestdone(state, hdl, obj, obex_cmd, obex_rsp);
                break;
            default:
              fprintf(stderr, "Unrecognized OBEX event encountered.\n");
              obex_requestdone(state, hdl, obj, obex_cmd, obex_rsp);
              return;
        }
    }
}

struct libusb_device_handle *find_smartpen() {
    // reference: http://www.dreamincode.net/forums/topic/148707-introduction-to-using-libusb-10/
    debug("entering find_smartpen()\n");
    libusb_device **devs = NULL; //pointer to pointer of device, used to retrieve a list of devices
    libusb_context *ctx = NULL; //a libusb session
    int r; // for return values
    ssize_t cnt; // holding number of devices in list
    debug("initializing libusb...\n");
    r = libusb_init(&ctx); // initialize a library session
    if(r) {
      fprintf(stderr, "Failed to initialize libusb.\n");
      return NULL;
    }
    libusb_set_debug(ctx, 3); // set verbosity level to 3, as suggested in the documentation
    debug("getting device list...\n");
    cnt = libusb_get_device_list(ctx, &devs); //get the list of devices
    if(cnt >= 0) {
        debug("device count: %d\n", (int) cnt);
        ssize_t i; // for iterating through the list
        struct libusb_device_descriptor descriptor; // for getting information on the devices
        debug("checking for livescribe pen...\n");
        for(i = 0; i < cnt; i++) {
            libusb_get_device_descriptor(devs[i],&descriptor);
            if ((descriptor.idVendor == LS_VENDOR_ID)) {
                 static struct libusb_device_handle *devHandle = NULL;
                libusb_open(devs[i], &devHandle);
                libusb_reset_device(devHandle);
                debug("exiting find_smartpen() returning device\n");
                return devHandle;
            }
        }
        debug("no devices match. freeing device list...\n");
        libusb_free_device_list(devs, 1); //free the list, unref the devices in it
        libusb_exit(ctx);
    } else {
      fprintf(stderr, "Error getting usb device list\n");
      libusb_free_device_list(devs, 1);
      libusb_exit(ctx);
      return NULL;
    }
    debug("exiting find_smartpen() returning NULL\n");
    return NULL;
}

const char* get_named_object(obex_t *handle, const char* name, uint32_t* len) {
    debug("attempting to retrieve named object \"%s\"...\n", name);
    struct obex_state* state;
    int req_done;
    obex_object_t *obj;
    obex_headerdata_t hd;
    int size, i;
    glong num;

    state = (struct obex_state*) OBEX_GetUserData(handle);
    OBEX_SetTransportMTU(handle, OBEX_MAXIMUM_MTU, OBEX_MAXIMUM_MTU);
    obj = OBEX_ObjectNew(handle, OBEX_CMD_GET);
    size = 4;

    hd.bq4 = state->connid;

    OBEX_ObjectAddHeader(handle, obj, OBEX_HDR_CONNECTION,
                         hd, size, OBEX_FL_FIT_ONE_PACKET);
    // converting name from utf8 to utf16
    hd.bs = (unsigned char *)g_utf8_to_utf16(name, strlen(name),
                                             NULL, &num, NULL);

    for (i=0; i<num; i++) {
        uint16_t *wchar = (uint16_t*)&hd.bs[i*2];
        *wchar = ntohs(*wchar);
    }
    size = (num+1) * sizeof(uint16_t);

    // Adding name header...   
    OBEX_ObjectAddHeader(handle, obj, OBEX_HDR_NAME, hd, size, OBEX_FL_FIT_ONE_PACKET);

    if (OBEX_Request(handle, obj) < 0) {
        OBEX_ObjectDelete(handle, obj);
        fprintf(stderr, "An error occured while OBEX retrieving an object. Returning null value.\n");
        return NULL;
    }

    req_done = state->req_done;
    
    while (state->req_done == req_done) {
        OBEX_HandleInput(handle, 100);
    }

    // done handling input
    if (state->body) {
        *len = state->body_len;
        // causes segfault and I'm not sure it's ever needed
    } else {
        *len = 0;
    }

    // TODO we never free this? Do we need an OBEX call or can we just call free?
    // check openobex api: http://dev.zuckschwerdt.org/openobex/doxygen/html/obex_8h.html
    return state->body;
}


static void smartpen_reset(short vendor, short product) {
    libusb_context *ctx;
    libusb_device_handle *dev;
    int rc;

    rc = libusb_init(&ctx);
    assert(rc == 0);
    dev = libusb_open_device_with_vid_pid(ctx, vendor, product);
    libusb_reset_device(dev);
    libusb_close(dev);
    libusb_exit(ctx);
}

obex_t *smartpen_connect(short vendor, short product) {
  obex_t* handle;
  obex_object_t* obj;
  int num, i;
  uint32_t rc;
  struct obex_state* state;
  obex_interface_t* obex_intf;
  obex_headerdata_t hd;
  int size, count;
  
  while(1) {
    handle = OBEX_Init(OBEX_TRANS_USB, obex_event, 0);
    if (!handle) {
      return NULL;
    }
    
    num = OBEX_FindInterfaces(handle, &obex_intf);
    for (i=0; i<num; i++) {
      if (!strcmp(obex_intf[i].usb.manufacturer, "Livescribe")) {
        break;
      }
    }
    
    if (i == num) {
      fprintf(stderr, "OBEX device not found\n");
      handle = NULL;
      return NULL;
    }
    
    state = (struct obex_state*) malloc(sizeof(struct obex_state));
    if (!state) {
      handle = NULL;
      return NULL;
    }
    memset(state, 0, sizeof(struct obex_state));
    
    swizzle_usb(vendor, product);
    
    rc = OBEX_InterfaceConnect(handle, &obex_intf[i]);
    if (rc < 0) {
      fprintf(stderr, "Failed to talk to smartpen. Is it already in use by another app?\n");
      handle = NULL;
      return NULL;
    }
    
    OBEX_SetUserData(handle, state);
    OBEX_SetTransportMTU(handle, 0x400, 0x400);
    
    obj = OBEX_ObjectNew(handle, OBEX_CMD_CONNECT);
    hd.bs = (unsigned char *) "LivescribeService";
    size = strlen((char*)hd.bs)+1;
    OBEX_ObjectAddHeader(handle, obj, OBEX_HDR_TARGET, hd, size, 0);
    
    rc = OBEX_Request(handle, obj);
    
    count = state->req_done;
    while (rc == 0 && state->req_done <= count) {
      OBEX_HandleInput(handle, 100);
    }
    
    if (rc < 0 || !state->got_connid) {
      debug("Retry connection...\n");
      OBEX_Cleanup(handle);
      continue;
    }
        
    const char* buf = get_named_object(handle, "ppdata?key=pp0000", &rc);
    if(!buf) {
      debug("Retry connection...\n");
      OBEX_Cleanup(handle);
      smartpen_reset(vendor, product);
      continue;
    }
    break;
  }
  return handle;
}

uint16_t identify_smartpen(struct libusb_device_handle* dev) {
  int ret;
  struct libusb_device_descriptor desc;

  ret = libusb_get_device_descriptor(libusb_get_device(dev), &desc);
  if(ret) {
    return 0;
  }

  debug("smartpen USB product ID: %x\n", desc.idProduct);

  if(is_ls_pulse(desc.idProduct)) {
    printf("LiveScribe Pulse smartpen detected!\n");
  } else if(is_ls_echo(desc.idProduct)) {
    printf("LiveScribe Echo smartpen detected!\n");
  } else {
    printf("Unknown LiveScribe device detected! Attempting to use this device anyways...\n");
  }

  return desc.idProduct;
}

// helper functionfor the extact function
int extract_copy_data(struct archive *ar, struct archive *aw) {
  int r;
  const void *buff;
  size_t size;
  off_t offset;

  for(;;) {
    r = archive_read_data_block(ar, &buff, &size, &offset);
    if(r == ARCHIVE_EOF)
      return ARCHIVE_OK;
    if(r < ARCHIVE_OK)
      return r;
    r = archive_write_data_block(aw, buff, size, offset);
    if(r < ARCHIVE_OK) {
      fprintf(stderr, "%s\n", archive_error_string(aw));
      return r;
    }
  }
}

int extract(const char* filename, const char* outdir) {
  struct archive *in;
  struct archive *out;
  struct archive_entry *entry;
  char oldwd[PATH_MAX];
  char* ret;
  int r;

  ret = getcwd(oldwd, PATH_MAX);
  if(!ret) {
    return 1;
  }

  in = archive_read_new();
  if(!in) {
    fprintf(stderr, "Could not initialize extractor.\n");
    return 1;
  }

  archive_read_support_filter_all(in);
  archive_read_support_format_all(in);
  r = archive_read_open_filename(in, filename, PATH_MAX-1);
  if(r != ARCHIVE_OK) {
    chdir(oldwd);
    return 1;
  }

  r = chdir(outdir);
  if(r) {
    fprintf(stderr, "Could not access output directory.\n");
    return 1;
  }

  out = archive_write_disk_new();
  if(!out) {
    fprintf(stderr, "Could not allocate archive extraction data structure.\n");
    return 1;
  }
  
  archive_write_disk_set_options(out, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_SECURE_NODOTDOT);

  while(archive_read_next_header(in, &entry) == ARCHIVE_OK) {

    r = archive_write_header(out, entry);
    if(r != ARCHIVE_OK) {
      chdir(oldwd);
      return 1;
    }
    
    r = extract_copy_data(in, out);
    if(r != ARCHIVE_OK) {
      chdir(oldwd);
      return 1;
    }    

    r = archive_write_finish_entry(out);
    if(r != ARCHIVE_OK) {
      chdir(oldwd);
      return 1;
    }
  }
  
  r = archive_read_close(in);
  if(r != ARCHIVE_OK) {
    chdir(oldwd);
    return 1;
  }
  r = archive_read_free(in);
  if(r != ARCHIVE_OK) {
    chdir(oldwd);
    return 1;
  }
  r = archive_write_close(out);
  if(r != ARCHIVE_OK) {
    chdir(oldwd);
    return 1;
  }
  r = archive_write_free(out);
  if(r != ARCHIVE_OK) {
    chdir(oldwd);
    return 1;
  }

  chdir(oldwd);
  return 0;
}


// retrieve data in zip format
int get_archive(obex_t *handle, char* object_name, const char* outfile, const char* outdir) {
	uint32_t len;
  uint32_t written;
  const char* buf;
  int ret;
  FILE* out;
  
  out = fopen(outfile, "w");
  if(!out) {
    fprintf(stderr, "Failed to open file for writing: %s\n", outfile);
    return 1;
  }
  
	buf = get_named_object(handle, object_name, &len);

  written = fwrite(buf, len, 1, out);
  if(!written) {
    fprintf(stderr, "Data could not be written to disk. Is your disk full?\n");
    return 1;
  }

  fclose(out);
  
  ret = extract(outfile, outdir);
  if(ret) {
    fprintf(stderr, "Failed to extract downloaded file.\n");
    unlink(outfile);
    return 1;
  }
  
  ret = unlink(outfile);
  if(ret) {
    fprintf(stderr, "Warning: Failed to delete file: %s.\n", outfile);
  }
  
  return 0;
}


// delete audio for specific notebook from pen
void delete_notebook_audio(obex_t* handle, char* doc_id, uint32_t* len) {
  char name[256];
        
  snprintf(name, sizeof(name), "lspcommand?name=Paper Replay&command=retire?docId=%s?copy=0?deleteSession=true", doc_id);
  return get_named_object(handle, name, &len);
}

// download all audio from pen
int get_audio(obex_t *handle, long long int start_time, const char* outfile, const char* outdir) {
	char name[256];


  printf("Downloading audio.\n");

  snprintf(name, sizeof(name), "lspdata?name=com.livescribe.paperreplay.PaperReplay&start_time=%lld&returnVersion=0.3&remoteCaller=WIN_LD_200", start_time);

  return get_archive(handle, name, outfile, outdir);
}

void delete_notebook_pages(obex_t* handle, char* doc_id, uint32_t* len) {
  char name[256];

  snprintf(name, sizeof(name), "lspcommand?name=%s&command=retire", doc_id);
  return get_named_object(handle, name, len);
}

int get_notebook_pages(obex_t *handle, const char* object_name, long long int start_time, const char* outfile, const char* outdir, int delete_after_get) {
	char name[256];
  uint32_t len;
  int ret;

  snprintf(name, sizeof(name), "lspdata?name=%s&start_time=%lld", object_name, start_time);

  ret = get_archive(handle, name, outfile, outdir);

  // if archive was extracted successfully, and delete_after_get is set
  // tell pen to delete the notebook
  if((ret == 0) && (delete_after_get > 0)) {
    delete_notebook_pages(handle, name, &len);
    delete_notebook_audio(handle, name, &len);
  }

  return ret;
}

// Get pen information xml
const char* get_peninfo(obex_t* handle, uint32_t* len) {
  const char* peninfo = get_named_object(handle, "peninfo", len);
  return peninfo;
}

// get system time in milliseconds
// NOTICE: This function returns < 0 on failure
long long int get_systemtime() {
  struct timespec t;
  int ret;

  ret = clock_gettime(CLOCK_REALTIME, &t);
  if(ret) {
    return -1;
  }
  
  return ((t.tv_sec * 1000) + (t.tv_nsec / 1000000));
}

// Pen time is reported in milliseconds but it is not clear
// what it is counting relative to.
// it is certainly not unix epoch time or anything obvious.
// It is not clear how to get the full datetime from the pen.
// NOTICE: This function returns < 0 on failure
long long int get_pentime(obex_t* handle) {
  long long int pentime = -1;
  uint32_t len;
  char* peninfo;
  xmlDocPtr doc;
  xmlXPathContextPtr xpathCtx; 
  xmlXPathObjectPtr xpathObj; 
  xmlNodeSetPtr nodes;
  xmlChar* val;
  int size;
  int i;

  const xmlChar* xpathExpr = BAD_CAST "/xml/peninfo/time";

  peninfo = get_peninfo(handle, &len);

  xmlInitParser();

  doc = xmlParseMemory(peninfo, len);
  if(!doc) {
    fprintf(stderr, "Failed to parse list of written pages.\n");
    return -1;
  }

  xpathCtx = xmlXPathNewContext(doc);
  if(!xpathCtx) {
    fprintf(stderr, "Failed to create XPath context.\n");
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return -1;
  }

  xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
  if(!xpathObj) {
    fprintf(stderr, "Failed to evaluate XPath expression.\n");
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return -1;
  }

  nodes = xpathObj->nodesetval;
  size = nodes->nodeNr;  
  if(size <= 0) {
    fprintf(stderr, "Pen info did not contain time value.\n");
    return -1;
  }

  for(i = 0; i < size; ++i) {
    if(nodes->nodeTab[i]->type != XML_ELEMENT_NODE) {
      continue;
    }
    val = xmlGetProp(nodes->nodeTab[i], BAD_CAST "absolute");
    if(val) {
      pentime = atoll((const char*) val);
      xmlFree(val);
      break;
    }
  }

  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  return pentime;
}

// Get difference between pen time and system time.
// This is important since pen time is relative to some
// weird non-standard and unknown reference point.
// It is not just milliseconds from January 1st 1970 :(
long long int get_time_offset(obex_t* handle) {
  long long int pentime;
  long long int systime;

  pentime = get_pentime(handle);
  if(pentime < 0) {
    return -1;
  }

  systime = get_systemtime(handle);
  if(systime < 0) {
    return -1;
  }

  return systime - pentime;
}

// write time offset to output dir
int write_time_offset(obex_t* handle, const char* outdir) {
  FILE* f;
  long long int offset;
  int written;
  char filepath[MAX_PATH_LENGTH];
  
  offset = get_time_offset(handle);
  if(offset < 0) {
    fprintf(stderr, "Failed to get time offset.\n");
    return 1;
  }

  snprintf(filepath, MAX_PATH_LENGTH, "%s/time_offset", outdir);
  
  f = fopen(filepath, "w");
  if(!f) {
    fprintf(stderr, "Failed to open time_offset for writing.\n");
    return 1;
  }

  written = fprintf(f, "%lld", offset);
  if(written <= 0) { 
    fprintf(stderr, "Failed to write time_offset.\n");
    return 1;
  }

  fclose(f);
  
  return 0;
}


// Get a list of written pages created since start_time
const char* get_written_page_list(obex_t* handle, long long int start_time, uint32_t* len) {
    char name[256];

    snprintf(name, sizeof(name), "changelist?start_time=%lld", start_time);
    return get_named_object(handle, name, len);
}

// Download all written pages
int get_all_written_pages(obex_t* handle, long long int start_time, const char* outdir, int delete_after_get) {

  uint32_t list_len;
  xmlDocPtr doc;
  xmlXPathContextPtr xpathCtx; 
  xmlXPathObjectPtr xpathObj; 
  xmlNodeSetPtr nodes;
  xmlChar* val;
  FILE* pagelistfile;
  int i, size;
  ssize_t written;
  char filepath[MAX_PATH_LENGTH];

  printf("Downloading written notes.\n");

  // This is safe since we know exactly what we're casting.
  const xmlChar* xpathExpr = BAD_CAST "/xml/changelist/lsp";

  const char* list = get_written_page_list(handle, start_time, &list_len);
  if(!list) {
    fprintf(stderr, "Failed to retrieve the list of written pages.\n");
    return 1;
  }

  snprintf(filepath, MAX_PATH_LENGTH, "%s/written_page_list.xml", outdir);
  
  pagelistfile = fopen(filepath, "w");
  if(!pagelistfile) {
    fprintf(stderr, "Failed to open written_page_list.xml for writing.\n");
    return 1;
  }

  written = fwrite(list, list_len, 1, pagelistfile);
  if(written <= 0) { 
    fprintf(stderr, "Failed to write written_page_list.xml.\n");
    return 1;
  }

  fclose(pagelistfile);

  // Cannot be sure this is a null terminated string so not safe to use debug()
  //  debug("Written page list:\n%s\n", list);

  xmlInitParser();

  doc = xmlParseMemory(list, list_len);
  if(!doc) {
    fprintf(stderr, "Failed to parse list of written pages.\n");
    xmlCleanupParser();
    return 1;
  }

  xpathCtx = xmlXPathNewContext(doc);
  if(!xpathCtx) {
    fprintf(stderr, "Failed to create XPath context.\n");
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 1;    
  }

  xpathObj = xmlXPathEvalExpression(xpathExpr, xpathCtx);
  if(!xpathObj) {
    fprintf(stderr, "Failed to evaluate XPath expression.\n");
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 1;
  }
  
  nodes = xpathObj->nodesetval;
  size = nodes->nodeNr;

  for(i = 0; i < size; ++i) {
    if(nodes->nodeTab[i]->type != XML_ELEMENT_NODE) {
      continue;
    }

    val = xmlGetProp(nodes->nodeTab[i], BAD_CAST "guid");
    if(!val) {
      continue;
    }
    debug("found notebook guid %s now retrievieving notebook pages\n", val);

    get_notebook_pages(handle, BAD_CAST val, start_time, "/tmp/dumpscribe_pages.zip", outdir, delete_after_get);

    xmlFree(val);
  }
  
  xmlXPathFreeObject(xpathObj);
  xmlXPathFreeContext(xpathCtx);
  xmlFreeDoc(doc);
  xmlCleanupParser();
  
  return 0;
}


void usage(const char* cmd_name) {
  printf("Usage: %s [-d] [-c] output_dir\n", cmd_name);
  printf("\n");
  printf("  -d: Enable debug output.\n");
  printf("  -c: Delete files from pen after successful download.\n");
  printf("\n");
}

int main(int argc, char** argv) {

  uint16_t usb_product_id;
  struct libusb_device_handle* dev;
  const char* audio_outfile = "/tmp/dumpscribe_audio.zip";
  int ret, opt, i;
  int extra_args = 0;
  char* output_dir;
  int clean_mode = 0;
  
  if(argc < 1) {
    usage("dumpscribe");
    return 1;
  }

  while((opt = getopt(argc, argv, "hd")) != -1) {
    switch (opt) {
      case 'h': 
        usage(argv[0]);
        return 0;
        break;
      case 'c':
        clean_mode = 1;
        break;
      case 'd':
        debug_mode = 1;
        break;
      default:        
        return 1;
    }
  }

  for(i = optind; i < argc; i++) {
    extra_args++;
    if(extra_args > 1) {
      usage(argv[0]);
      return 1;
    }
    output_dir = argv[i];
  }

  if(extra_args < 1) {
    usage(argv[0]);
    return 1;
  }

  dev = find_smartpen();
  if(!dev) {
    fprintf(stderr, "No smartpen found. Are you sure it's connected?\n");
    return 1;
  }
  
  usb_product_id = identify_smartpen(dev);
  if(!usb_product_id) {
    fprintf(stderr, "Failed to get USB device descriptor.\n");
    return 1;
  }
  
  obex_t* handle = smartpen_connect(LS_VENDOR_ID, usb_product_id);

  if(!handle) {
    fprintf(stderr, "Failed to open device\n");
    return 1;
  }

  printf("Connected to smartpen!\n");

  ret = get_audio(handle, 0, audio_outfile, output_dir);
  if(ret) {
    fprintf(stderr, "Failed to download audio from smartpen.\n");
    return 1;
  }

  ret = get_all_written_pages(handle, 0, output_dir, clean_mode);
  if(ret) {
    fprintf(stderr, "Failed to get list of written pages from smartpen.\n");
    return 1;
  }

  ret = write_time_offset(handle, output_dir);
  if(ret) {
    fprintf(stderr, "Failed to write time offset.\n");
    return 1;
  }

  return 0;
}
