
LOCAL_CFLAGS=-Wall -fPIC `pkg-config --cflags glib-2.0 openobex libusb-1.0` -g

all: dumpscribe

%.o: %.c
	gcc -o $@ -c $^ $(CFLAGS) $(LOCAL_CFLAGS)

dumpscribe: dumpscribe.o
	gcc -o $@ $^ `pkg-config --libs glib-2.0 openobex libusb-1.0`

clean:
	rm *.o dumpscribe
