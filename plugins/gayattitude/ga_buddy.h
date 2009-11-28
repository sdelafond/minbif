#ifndef GA_GABUDDY_H
#define GA_GABUDDY_H

#include <purple.h>
#include "ga_account.h"
#include "ga_parsing.h"


typedef struct _GayAttitudeBuddy GayAttitudeBuddy;
typedef struct _GayAttitudeBuddyInfoRequest GayAttitudeBuddyInfoRequest;

typedef void (*GayAttitudeRequestInfoCallbackFunc)(GayAttitudeAccount* gaa, gpointer user_data);

struct _GayAttitudeBuddy {
	PurpleBuddy		*buddy;
	gchar			*ref_id;
};

struct _GayAttitudeBuddyInfoRequest {
	GayAttitudeBuddy			*gabuddy;
	gboolean				advertise;
	GayAttitudeRequestInfoCallbackFunc	callback;
	gpointer				callback_data;
};


GayAttitudeBuddy *ga_get_gabuddy_from_buddy(PurpleBuddy *buddy, gboolean create);
GayAttitudeBuddy *ga_find_gabuddy(GayAttitudeAccount *gaa, const gchar *gabuddyname);
GayAttitudeBuddy *ga_gabuddy_new(GayAttitudeAccount *gaa, const gchar *buddyname);
void ga_gabuddy_free(GayAttitudeBuddy *gabuddy);
void ga_request_info(GayAttitudeAccount* gaa, const char *who, gboolean advertise, GayAttitudeRequestInfoCallbackFunc callback, gpointer callback_data);

#endif /* GA_GABUDDY_H */