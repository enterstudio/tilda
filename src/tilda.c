/*
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib-object.h>
#include <stdio.h>
#include <signal.h>

#ifdef HAVE_XFT2
#include <fontconfig/fontconfig.h>
#endif

#include <vte/vte.h>
#include "config.h"
#include "tilda.h"
#include "tilda_window.h"

char *user, *display;

void clean_up_no_args ()
{
     exit (0);
}

/* Removes the temporary file socket used to communicate with a running tilda */
void clean_up (tilda_window *tw)
{
    remove (tw->lock_file);

    exit (0);
}

void getinstance (tilda_window *tw)
{
    char *home_dir;
	char buf[1024];
	char filename[1024], tmp[100];
    FILE *ptr;
	
    tw->instance = 0;
	
	home_dir = getenv ("HOME");
    strlcpy (tw->lock_file, home_dir, sizeof(tw->lock_file));
    strlcat (tw->lock_file, "/.tilda", sizeof(tw->lock_file));
    strlcat (tw->lock_file, "/locks/", sizeof(tw->lock_file));
	
	mkdir (tw->lock_file,  S_IRUSR | S_IWUSR | S_IXUSR);
	
	for (;;)
    {
		strlcpy (tmp, "ls ~/.tilda/locks/lock", sizeof(tmp));
        sprintf (filename, "%s_*_%d", tmp, tw->instance);
  		sprintf (filename, "%s 2> /dev/null", filename);
  
        if ((ptr = popen (filename, "r")) != NULL)
        {
            if (fgets (buf, BUFSIZ, ptr) != NULL)
                tw->instance++;
            else
            {
                pclose (ptr);
                break;
            }
            pclose (ptr);
        } 
    }

	sprintf (filename, "%slock_%d_%d", tw->lock_file, getpid(), tw->instance);
    strlcpy (tw->lock_file, filename, sizeof (tw->config_file));
    creat (tw->lock_file, S_IRUSR | S_IWUSR | S_IXUSR);
}

void clean_tmp ()
{
	char *home_dir;
	char pid[10];
    char cmd[128];
    char buf[1024], filename[1024];
	char tmp[100];
	char *throw_away;
    int  length, i, instance;
    FILE *ptr, *ptr2;
    int error_to_null, x;
	gboolean old = TRUE;
	
    home_dir = getenv ("HOME");
	strlcpy (cmd, "ls ", sizeof(cmd));
    strlcat (cmd, home_dir, sizeof(cmd));
    strlcat (cmd, "/.tilda/locks/lock_* 2> /dev/null", sizeof(cmd));
 
    if ((ptr = popen (cmd, "r")) != NULL)
    {
        while (fgets (buf, 1024, ptr) != NULL)
        {
			strlcpy (filename, buf, sizeof (filename));
			throw_away = strtok (buf, "/");
			while (throw_away)
			{
				strlcpy (tmp, throw_away, sizeof (tmp));
				throw_away = strtok (NULL, "/");
				
				if (!throw_away)
				{
					throw_away = strtok (tmp, "_");
					throw_away = strtok (NULL, "_");
					strlcpy (pid, throw_away, sizeof (pid));
					break;
				}
			}
			
			strlcpy (cmd, "ps x | grep ", sizeof(cmd));
			strlcat (cmd, pid, sizeof (cmd));
			
			if ((ptr2 = popen (cmd, "r")) != NULL)
			{
				while (fgets (tmp, 1024, ptr2) != NULL)
				{
					if (strstr (tmp, "tilda") != NULL)
					{
						old = FALSE;
						break;
					}
				}

				if (old)
				{
					filename[strlen(filename)-1] = '\0';
					remove (filename);
				}
				else 
					old = TRUE;			
					
				pclose (ptr2);
			}
        } 
    }
    
    pclose (ptr);
}

int main (int argc, char **argv)
{
	tilda_window *tw;
    tilda_term *tt;

    const char *command = NULL;
    const char *working_directory = NULL;
    char *home_dir;
    FILE *fp;
    float tmp_val;
    int  opt;
    int  i, j;
    GList *args = NULL;
	
	/* Gotta do this first to make sure no lock files are left over */
	clean_tmp ();

	/* create new tilda window and terminal */
    tw = (tilda_window *) malloc (sizeof (tilda_window));
	tw->tc = (tilda_conf *) malloc (sizeof (tilda_conf));
	tt = (tilda_term *) malloc (sizeof (tilda_term));

	init_tilda_window_configs (tw);

    /* Have to do this early. */
    if (getenv ("VTE_PROFILE_MEMORY"))
    {
        if (atol (getenv ("VTE_PROFILE_MEMORY")) != 0)
        {
            g_mem_set_vtable (glib_mem_profiler_table);
        }
    }

    /* Pull out long options for GTK+. */
    for (i=j=1;i<argc;i++)
    {
        if (g_ascii_strncasecmp ("--", argv[i], 2) == 0)
        {
            args = g_list_append (args, argv[i]);

            for (j=i;j<argc;j++)
                argv[j] = argv[j + 1];

            argc--;
            i--;
        }
    }

    /* set the instance number and place a env in the array of envs
    * to be set when the tilda terminal is created */
    getinstance (tw);

    /*check for -T argument, if there is one just write to the pipe and exit, this will bring down or move up the term*/
    while ((opt = getopt(argc, argv, "x:y:B:CDTab:c:df:ghkn:st:wl:-")) != -1)
     {
        gboolean bail = FALSE;
        switch (opt) {
            case 'B':
                image_set_clo = TRUE;
                strlcpy (tw->tc->s_image, optarg, sizeof (tw->tc->s_image));
                break;
            case 'b':
                strlcpy (tw->tc->s_background, optarg, sizeof (tw->tc->s_background));
                break;
            case 'T':
                printf ("-T no longer does anything :(, tilda no longer uses xbindkeys\n");
                printf ("If there is a demand I can fix it up so both new and old work.\n");
                printf ("I see this as extra overhead for no reason however, sorry.\n");
                break;
            case 'C':
                if ((wizard (argc, argv, tw, tt)) == 1) { clean_up(tw); }
                break;
            case 's':
                scroll_set_clo = TRUE;
                break;
            case 'c':
                command = optarg;
                break;
            case 't':
                tmp_val = atoi (optarg);
                if (tmp_val <= 100 && tmp_val >=0 ) { TRANS_LEVEL_arg = ((double) tmp_val)/100; }
                break;
            case 'f':
                strlcpy (s_font_arg, optarg, sizeof (tw->tc->s_font));
                break;
            case 'w':
                working_directory = optarg;
                break;
            case 'a':
                antialias_set_clo = TRUE;
                break;
            case 'h':
                g_print(usage, argv[0]);
                exit(1);
                break;
            case 'l':
                tw->tc->lines = atoi (optarg);

                if (tw->tc->lines <= 0)
                    tw->tc->lines = DEFAULT_LINES;

                break;
            case '-':
                bail = TRUE;
                break;
            case 'x':
                x_pos_arg = atoi (optarg);
                break;
            case 'y':
                y_pos_arg = atoi (optarg);
                break;
            default:
                break;
        }
        if (bail)
            break;
    }

    home_dir = getenv ("HOME");
    strlcpy (tw->config_file, home_dir, sizeof(tw->config_file));
    strlcat (tw->config_file, "/.tilda/config", sizeof(tw->config_file));
    sprintf (tw->config_file, "%s_%i", tw->config_file, tw->instance);

    /* Call the wizard if we cannot read the config file.
     * This fixes a crash that happened if there was not a config file, and
     * tilda was not called with "-C" */
    if ((fp = fopen(tw->config_file, "r")) == NULL)
    {
        printf("Unable to open config file, showing the wizard\n");
        if ((wizard (argc, argv, tw, tt)) == 1) { clean_up(tw); }
    }
    else
        fclose (fp);

    if (read_config_file (argv[0], tw->tilda_config, NUM_ELEM, tw->config_file) < 0)
    {
        puts("There was an error in the config file, terminating");
        exit(1);
    }

    if (strcasecmp (tw->tc->s_key, "null") == 0)
        sprintf (tw->tc->s_key, "None+F%i", tw->instance+1);

    g_thread_init(NULL);
    gdk_threads_init();

    gtk_init (&argc, &argv);

	init_tilda_window (tw, tt);

    /*signal (SIGINT, clean_up);
    signal (SIGQUIT, clean_up);
    signal (SIGABRT, clean_up);
    signal (SIGKILL, clean_up);
    signal (SIGABRT, clean_up);
    signal (SIGTERM, clean_up);*/

    gdk_threads_enter ();
    gtk_main();
    gdk_threads_leave ();

    printf ("remove %s\n", tw->lock_file);
    remove (tw->lock_file);

    return 0;
}

