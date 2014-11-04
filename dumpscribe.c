#include <openobex/obex.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <glib.h>
#include <libusb.h>
#include <assert.h>
#include <arpa/inet.h>
#include <stdio.h>

#define LS_VENDOR_ID 0x1cfb //LiveScribe Vendor ID
inline int is_ls_pulse(unsigned int c) { return (c == 0x1020 || c == 0x1010); } //LiveScribe Pulse(TM) Smartpen
inline int is_ls_echo(unsigned int c) { return c == 0x1030 || c == 0x1032; } //LiveScribe Echo(TM) Smartpen

struct obex_state {
    obex_t *handle;
    int req_done;
    char *body;
    int body_len;
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

// TODO what the hell is this?
void swizzle_usb(short vendor, short product) {
    libusb_context* ctx;
    libusb_device_handle* dev;
    int rc;

    rc = libusb_init(&ctx);
    assert(rc == 0);
    dev = libusb_open_device_with_vid_pid(ctx, vendor, product);
    assert(dev);

    libusb_set_configuration(dev, 1);
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
//                  printf("Connection ID: %d\n", state->connid);
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
            printf("oah fail %d\n", rc);
        }
    } else if (obex_rsp != OBEX_RSP_SUCCESS && obex_rsp != OBEX_RSP_CONTINUE) {
      fprintf(stderr, "Unrecognized OBEX event encountered. Ignoring.\n");
    } else {
        switch (event) {
            case OBEX_EV_REQDONE:
                obex_requestdone(state, hdl, obj, obex_cmd, obex_rsp);
                break;
            default:
              fprintf(stderr, "Unrecognized OBEX event encountered. Ignoring.\n");
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

const char* get_named_object(obex_t *handle, const char* name, int* len) {
    printf("attempting to retrieve named object \"%s\"...\n", name);
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
        OBEX_ObjectDelete(handle,obj);
        printf("an error occured while retrieving the object. returning null value.\n");
        return NULL;
    }

    req_done = state->req_done;
    while (state->req_done == req_done) {
        OBEX_HandleInput(handle, 100);
    }
    // done handling input
    if (state->body) {
        *len = state->body_len;
        state->body[state->body_len] = '\0';
    } else {
        *len = 0;
    }
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
  int rc, num, i;
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
      printf("Retry connection...\n");
      OBEX_Cleanup(handle);
      continue;
    }
        
    const char* buf = get_named_object(handle, "ppdata?key=pp0000", &rc);
    if(!buf) {
      printf("Retry connection...\n");
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


int main (void) {

  uint16_t usb_product_id;
  struct libusb_device_handle* dev;

  // TODO take command line argument for this
  debug_mode = 1;

  dev = find_smartpen();
  if(!dev) {
    fprintf(stderr, "No smartpen found. Are you sure it's connected?");
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

  /*

  char *changelist;
  int rc;
   
  changelist = smartpen_get_changelist(handle, 0);
  
  printf("Changelist: %s\n", changelist);
  printf("Done\n");
  
  changelist = smartpen_get_peninfo(handle);
  
  printf("Peninfo: %s\n", changelist);
  printf("Done\n");
  
  FILE *out = fopen("data", "w");
  
  rc = smartpen_get_guid(handle, out, "0x0bf11a726d11f3f3", 0);
  if (!rc) {
    printf("get_guid fail\n");
  }
  
  smartpen_get_paperreplay(handle, out, 0);
  
  fclose(out);
  
  smartpen_disconnect(handle);
  return 0;
  */

  return 0;
}