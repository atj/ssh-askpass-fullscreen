/*  File: gtk2-ssh-askpass.c 
 * Copyright (C) 2002-2003 Christopher R. Gabriel <cgabriel@cgabriel.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 *
 *
 * This is a simple GTK2.0 SSH passphrase grabber. To use it, set the 
 * environment variable SSH_ASKPASS to point to the location of 
 * gnome-ssh-askpass before calling "ssh-add < /dev/null". 
 *
 * There is only two run-time options: if you set the environment variable
 * "GNOME_SSH_ASKPASS_GRAB_SERVER=true" then gnome-ssh-askpass will grab
 * the X server. If you set "GNOME_SSH_ASKPASS_GRAB_POINTER=true", then the 
 * pointer will be grabbed too. These may have some benefit to security if 
 * you don't trust your X server. We grab the keyboard always.
 *
 *
 * Based on gnome-ssh-askpass from the OpenSSH package.
 *
 */

/* VERSION 0.4 */


/*
 * Compile with:
 *
 * cc `pkg-config --cflags gtk+-2.0` `pkg-config --libs gtk+-2.0` \ 
 *     -o gtk2-ssh-askpass gtk2-ssh-askpass.c
 *
 */

#define GRAB_TRIES      16
#define GRAB_WAIT       250 /* milliseconds */   

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <X11/Xlib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

/* XPM */
static const char *ocean_stripes[] = {
/* columns rows colors chars-per-pixel */
"64 64 3 1",
"  c #A0A9C1",
". c #B3BCC8",
"X c #CCD0D7",
/* pixels */
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                ",
"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
"................................................................",
"                                                                ",
"                                                                "
};


GtkWidget *window, *label, *entry;
gint grab_server, grab_pointer;

static void
report_failed_grab (const char *what)
{
	GtkWidget *err;

	err = gtk_message_dialog_new(NULL, 0,
				     GTK_MESSAGE_ERROR,
				     GTK_BUTTONS_CLOSE,
				     "Could not grab %s. "
				     "A malicious client may be eavesdropping "
				     "on your session.", what);
	gtk_window_set_position(GTK_WINDOW(err), GTK_WIN_POS_CENTER);
	gtk_label_set_line_wrap(GTK_LABEL((GTK_MESSAGE_DIALOG(err))->label),
				TRUE);

	gtk_dialog_run(GTK_DIALOG(err));

	gtk_widget_destroy(err);
}




static GdkPixbuf *
create_tile_pixbuf (GdkPixbuf    *dest_pixbuf,
		    GdkPixbuf    *src_pixbuf,
		    GdkRectangle *field_geom,
		    guint         alpha,
		    GdkColor     *bg_color) 
{
	gboolean need_composite;
	gboolean use_simple;
	gdouble  cx, cy;
	gdouble  colorv;
	gint     pwidth, pheight;

	need_composite = (alpha < 255 || gdk_pixbuf_get_has_alpha (src_pixbuf));
	use_simple = (dest_pixbuf == NULL);

	if (dest_pixbuf == NULL)
		dest_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, field_geom->width, field_geom->height);

	if (need_composite && use_simple)
		colorv = ((bg_color->red & 0xff00) << 8) |
			(bg_color->green & 0xff00) |
			((bg_color->blue & 0xff00) >> 8);
	else
		colorv = 0;

	pwidth = gdk_pixbuf_get_width (src_pixbuf);
	pheight = gdk_pixbuf_get_height (src_pixbuf);

	for (cy = 0; cy < field_geom->height; cy += pheight) {
		for (cx = 0; cx < field_geom->width; cx += pwidth) {
			if (need_composite && !use_simple)
				gdk_pixbuf_composite
					(src_pixbuf, dest_pixbuf,
					 cx, cy,
					 MIN (pwidth, field_geom->width - cx), 
					 MIN (pheight, field_geom->height - cy),
					 cx, cy,
					 1.0, 1.0,
					 GDK_INTERP_BILINEAR,
					 alpha);
			else if (need_composite && use_simple)
				gdk_pixbuf_composite_color
					(src_pixbuf, dest_pixbuf,
					 cx, cy,
					 MIN (pwidth, field_geom->width - cx), 
					 MIN (pheight, field_geom->height - cy),
					 cx, cy,
					 1.0, 1.0,
					 GDK_INTERP_BILINEAR,
					 alpha,
					 65536, 65536, 65536,
					 colorv, colorv);
			else
				gdk_pixbuf_copy_area
					(src_pixbuf,
					 0, 0,
					 MIN (pwidth, field_geom->width - cx),
					 MIN (pheight, field_geom->height - cy),
					 dest_pixbuf,
					 cx, cy);
		}
	}

	return dest_pixbuf;
}

void
enter_callback(GtkWidget *widget,
               GtkWidget *entryw)
{
  const gchar *passphrase;
  passphrase = gtk_entry_get_text(GTK_ENTRY(entryw));

  if (grab_server) 
	  XUngrabServer(GDK_DISPLAY()); 
  if (grab_pointer) 
	  gdk_pointer_ungrab(GDK_CURRENT_TIME); 
  gdk_keyboard_ungrab(GDK_CURRENT_TIME);
  gdk_flush();
  
  puts(passphrase);
		
  memset((void*)passphrase, '\0', strlen(passphrase)); 
  gtk_entry_set_text(GTK_ENTRY(entry), passphrase);
  gtk_main_quit();
  
}

void
passphrase_dialog(char *message)
{
	GtkWidget *frame, *align, *vbox, *hbox;
	GdkPixbuf *tmp_pixbuf, *pixbuf, *tile_pixbuf;
	GdkPixmap *pixmap;
	GdkRectangle rect;
	GdkColor color;
	gchar *str;
        GdkGrabStatus status;
        int grab_tries = 0;
        const char *failed;
        
 	grab_server = (getenv("GNOME_SSH_ASKPASS_GRAB_SERVER") != NULL); 
 	grab_pointer = (getenv("GNOME_SSH_ASKPASS_GRAB_POINTER") != NULL); 

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size(GTK_WINDOW(window),
                                    gdk_screen_width(),
                                    gdk_screen_height());

	gtk_widget_set_app_paintable(GTK_WIDGET(window), TRUE);
	gtk_widget_realize(GTK_WIDGET(window));

	tmp_pixbuf = gdk_pixbuf_get_from_drawable(NULL,
                                                  gdk_get_default_root_window(),
                                                  gdk_colormap_get_system(),
                                                  0,
                                                  0,
                                                  0,
                                                  0,
                                                  gdk_screen_width(),
                                                  gdk_screen_height());

	pixbuf = gdk_pixbuf_new_from_xpm_data(ocean_stripes);
	
	rect.x = 0;
	rect.y = 0;
	rect.width = gdk_screen_width();
	rect.height = gdk_screen_height();

	color.red = 0;
	color.blue = 0;
	color.green = 0;

 	tile_pixbuf = create_tile_pixbuf(NULL,
                                         pixbuf,
                                         &rect,
                                         155,
                                         &color);
        
	g_object_unref(pixbuf);
	
	gdk_pixbuf_composite(tile_pixbuf,
                             tmp_pixbuf,
                             0,
                             0,
                             gdk_screen_width(),
                             gdk_screen_height(),
                             0,
                             0,
                             1,
                             1,
                             GDK_INTERP_NEAREST,
                             200);

	g_object_unref(tile_pixbuf);

	pixmap = gdk_pixmap_new(GTK_WIDGET(window)->window,
                                gdk_screen_width(),
                                gdk_screen_height(),
                                -1);

	gdk_pixbuf_render_to_drawable_alpha(tmp_pixbuf,
                                            pixmap,
                                            0,
                                            0,
                                            0,
                                            0,
                                            gdk_screen_width(),
                                            gdk_screen_height(),
                                            GDK_PIXBUF_ALPHA_BILEVEL,
                                            0,
                                            GDK_RGB_DITHER_NONE,
                                            0,
                                            0);

	g_object_unref(tmp_pixbuf);
	
	gdk_window_set_back_pixmap(GTK_WIDGET(window)->window, pixmap, FALSE);
	g_object_unref(pixmap);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_widget_show(frame);

	align = gtk_alignment_new(0.5,0.5,0.0,0.0);
	gtk_widget_show(align);

	gtk_container_add(GTK_CONTAINER(window), align);
	vbox = gtk_vbox_new(FALSE,0);
	gtk_widget_show(vbox);

	gtk_container_add(GTK_CONTAINER(align), frame);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	label = gtk_label_new(NULL);
	gtk_widget_show(label);

	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 12);

	str = g_strdup_printf("<span foreground=\"white\" size=\"xx-large\"><b>%s</b></span>",
						  message);
	gtk_label_set_markup(GTK_LABEL(label), str);
	g_free(str);
	
	hbox = gtk_hbox_new(FALSE,0);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_widget_show(entry);
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 8);
	g_signal_connect(G_OBJECT(entry), "activate",
					  G_CALLBACK(enter_callback),
					  (gpointer) entry);

 	gtk_window_stick(GTK_WINDOW(window)); 
	gtk_widget_grab_focus(entry);	 
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

#if GTK_MINOR_VERSION > 0
	gtk_window_fullscreen(GTK_WINDOW(window));
#endif

        gtk_widget_show(GTK_WIDGET(window));
        
        /* Grab focus */
	if (grab_pointer) {
		for(;;) {
			status = gdk_pointer_grab(window->window, TRUE,
                                                  0, NULL, NULL, GDK_CURRENT_TIME);
			if (status == GDK_GRAB_SUCCESS)
				break;
			usleep(GRAB_WAIT * 1000);
			if (++grab_tries > GRAB_TRIES) {
				failed = "mouse";
				goto nograb;
			}
		}
	}
	for(;;) {
		status = gdk_keyboard_grab((window)->window, FALSE,
                                           GDK_CURRENT_TIME);
		if (status == GDK_GRAB_SUCCESS)
			break;
		usleep(GRAB_WAIT * 1000);
		if (++grab_tries > GRAB_TRIES) {
			failed = "keyboard";
			goto nograbkb;
		}
	}
	if (grab_server) {
		gdk_x11_grab_server();
	}
        

			
	return;

 nograbkb: 
 	gdk_pointer_ungrab(GDK_CURRENT_TIME); 

 nograb: 
 	if (grab_server) 
	  XUngrabServer(GDK_DISPLAY()); 

        report_failed_grab(failed);

        gtk_main_quit();
}



int
main(int argc, char **argv)
{
	gchar *message;

	gtk_init(&argc, &argv);

	if (argc == 2)
		message = argv[1];
	else
		message = "Enter your OpenSSH passphrase:";

	setvbuf(stdout, 0, _IONBF, 0);
	passphrase_dialog(message);
	gtk_main();
	return 0;

}
