#include "ddcbc-api/ddcbc-api.c"
#include <regex.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>

int get_value(char *arg, ddcbc_display *disp) {
	GError *err = NULL;
	GMatchInfo *mi = NULL;
	int new_val = 0;

	GRegex *percentage_diff = g_regex_new("(\\+|-)(\\d+)%", 0, 0, &err);
	GRegex *num_diff = g_regex_new("(\\+|-)(\\d+)", 0, 0, &err);
	GRegex *percentage_only = g_regex_new("(\\d+)%", 0, 0, &err);
	GRegex *num_only = g_regex_new("(\\d+)", 0, 0, &err);

	if (err != NULL) {
		puts("Setting one or more regular expression objects failed.");
		return -1;
	}

	if (g_regex_match(percentage_diff, arg, 0, &mi)) {
		char *sign = g_match_info_fetch(mi, 1);
		guint percentage = atoi(g_match_info_fetch(mi, 2));
		
		double multiplier = ((double) percentage / 100.0);
		ddcbc_display_get_brightness(disp);
		double inc_dbl = (double) disp->max_val * multiplier;
		
		new_val = strcmp(sign, "+") == 0 ? disp->last_val + (int) inc_dbl : disp->last_val - (int) inc_dbl;
		
	} else if (g_regex_match(num_diff, arg, 0, &mi)) {
		char *sign = g_match_info_fetch(mi, 1);
		guint inc = atoi(g_match_info_fetch(mi, 2));

		new_val = strcmp(sign, "+") == 0 ? new_val + inc : new_val - inc;
	} else if (g_regex_match(percentage_only, arg, 0, &mi)) {
		guint percentage = atoi(g_match_info_fetch(mi, 1));
		double multiplier = ((double) percentage / 100.0);
		new_val = disp->max_val * multiplier;
	} else if (g_regex_match(num_only, arg, 0, &mi)) {
		new_val = atoi(g_match_info_fetch(mi, 1));
	} else {
		new_val = 0;
	}

	if (new_val < 0) {
		return 0;
	} else if (new_val > disp->max_val) {
		return disp->max_val;
	}
	
	return new_val;
}

typedef struct {
	ddcbc_display *disp;
	int new_val;
} brightness_setter;

void *set_brightness(void *bright_set) {
	brightness_setter *bset = bright_set;

	printf("Setting dispno %u to val %d\n", bset->disp->info.dispno, bset->new_val);
	DDCBC_Status rc = ddcbc_display_set_brightness(bset->disp, bset->new_val);
	if (rc != 0)
		printf("Error Code: %d", rc);

	pthread_exit(NULL);	
}

// set_all_displays concurrently sets the brightness of all displays in 'dlist' to value described in 'arg'.
int set_all_displays(ddcbc_display_list *dlist, char *arg) {
	pthread_t threads[dlist->ct];
	brightness_setter *bset;

	for (guint i = 0; i < dlist->ct; i++) {
		ddcbc_display *disp = ddcbc_display_list_get(dlist, i);
				
		int new_val = get_value(arg, ddcbc_display_list_get(dlist, i));
		if (new_val < 0) {
			puts("Invalid brightness value!");
			return 1;
		}

		bset = malloc(sizeof(brightness_setter));
		bset->disp = disp;
		bset->new_val = new_val;

		int rc = pthread_create(&threads[i], NULL, set_brightness, (void *) bset);
		if (rc) {
			puts("Error creating thread.");
			return -1;
		}
	}

	for (guint i = 0; i < dlist->ct; i++) {
		pthread_join(threads[i], NULL);
	}
	free(bset);

	return 0;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		puts("Must provide at least two arguments.");
		return 1;
	}

	ddcbc_display_list dlist = ddcbc_display_list_init(FALSE);
	guint dispno = atoi(argv[1]);
	if (dispno == 0) {
		if (strcmp("all", argv[1]) == 0) {
			set_all_displays(&dlist, argv[2]);
		} else if (dispno <= dlist.ct) {
			ddcbc_display *disp = ddcbc_display_list_get(&dlist, dispno - 1);
			int new_val = get_value(argv[2], ddcbc_display_list_get(&dlist, dispno - 1));
			DDCBC_Status rc = ddcbc_display_set_brightness(disp, new_val);
			if (rc != 0)
				printf("Error Code: %d", rc);
		} else {
			puts("Invalid display number.");
			return 1;
		}
	}
}