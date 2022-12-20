/* Copyright (C) 2002-2003 Christopher R. Gabriel <cgabriel@cgabriel.org>
 * Copyright (C) 2009 Adam James <atj@pulsewidth.org.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version. 
 */

#define GRAB_TRIES	16
#define GRAB_WAIT	250 /* milliseconds */

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
#include <gdk/gdkkeysyms.h>

#ifdef __GNUC__
#define ATTR_UNUSED	__attribute__((unused))
#else
#define ATTR_UNUSED
#endif

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
int exit_code;

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
	gdouble cx, cy;
	gdouble colorv;
	gint pwidth, pheight;

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
ungrab()
{
	if (grab_server) 
		XUngrabServer(GDK_DISPLAY()); 
	if (grab_pointer) 
		gdk_pointer_ungrab(GDK_CURRENT_TIME); 

	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_flush();
}


void
enter_callback(GtkWidget *widget ATTR_UNUSED,
               GtkWidget *entryw)
{
	const gchar *passphrase;
	passphrase = gtk_entry_get_text(GTK_ENTRY(entryw));

	ungrab();

	puts(passphrase);
		
	memset((void*)passphrase, '\0', strlen(passphrase)); 
	gtk_entry_set_text(GTK_ENTRY(entry), passphrase);
	gtk_main_quit();
}

void
cancel()
{
	ungrab();
	exit_code = 1;
	gtk_main_quit();
}

gboolean
key_press_callback(GtkWidget *widget ATTR_UNUSED,
               GdkEventKey *event,
               GtkWidget *entryw ATTR_UNUSED)
{
	gboolean rv = FALSE;
	switch (event->keyval)
	{
		case GDK_KEY_Escape:
			cancel();
			rv = TRUE;
			break;
	}

	return rv;
}

void
passphrase_dialog(char *message)
{
	GtkWidget *frame, *align, *vbox, *hbox;
	GdkPixbuf *tmp_pixbuf, *pixbuf, *tile_pixbuf;
	GdkPixmap *pixmap;
	GdkRectangle rect;
	GdkColor color;
	gchar *escaped_message, *str;
	GdkGrabStatus status;
	int grab_tries = 0;
	const char *failed;
	GdkScreen *screen = gdk_screen_get_default();
	GdkWindow *active_window;
	gint monitor_num;
	GdkRectangle monitor_geometry;

	grab_server = (getenv("GNOME_SSH_ASKPASS_GRAB_SERVER") != NULL);
	grab_pointer = (getenv("GNOME_SSH_ASKPASS_GRAB_POINTER") != NULL);

	active_window = gdk_screen_get_active_window(screen);
	monitor_num = gdk_screen_get_monitor_at_window(screen, active_window);
	gdk_screen_get_monitor_geometry(screen, monitor_num, &monitor_geometry);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_window_set_default_size(GTK_WINDOW(window),
				    monitor_geometry.width,
				    monitor_geometry.height);

	gtk_widget_set_app_paintable(GTK_WIDGET(window), TRUE);
	gtk_widget_realize(GTK_WIDGET(window));

	tmp_pixbuf = gdk_pixbuf_get_from_drawable(NULL,
						  gdk_screen_get_root_window(screen),
						  gdk_colormap_get_system(),
						  monitor_geometry.x,
						  monitor_geometry.y,
						  0,
						  0,
						  monitor_geometry.width,
						  monitor_geometry.height);

	pixbuf = gdk_pixbuf_new_from_xpm_data(ocean_stripes);
	
	rect.x = 0;
	rect.y = 0;
	rect.width = monitor_geometry.width;
	rect.height = monitor_geometry.height;

	color.red = 0;
	color.blue = 0;
	color.green = 0;

 	tile_pixbuf = create_tile_pixbuf(NULL, pixbuf,
					 &rect, 155,
					 &color);

	g_object_unref(pixbuf);
	
	gdk_pixbuf_composite(tile_pixbuf, tmp_pixbuf,
			     0, 0,
			     monitor_geometry.width,
			     monitor_geometry.height,
			     0, 0, 1, 1,
			     GDK_INTERP_NEAREST, 200);

	g_object_unref(tile_pixbuf);

	pixmap = gdk_pixmap_new(GTK_WIDGET(window)->window,
				monitor_geometry.width,
				monitor_geometry.height,
				-1);

	gdk_pixbuf_render_to_drawable_alpha(tmp_pixbuf,
					    pixmap,
					    0, 0, 0, 0,
					    monitor_geometry.width,
					    monitor_geometry.height,
					    GDK_PIXBUF_ALPHA_BILEVEL,
					    0, GDK_RGB_DITHER_NONE,
					    0, 0);

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

	escaped_message = g_markup_escape_text(message, strlen(message));
	str = g_strdup_printf("<span foreground=\"white\" size=\"xx-large\"><b>%s</b></span>",
						  escaped_message);
	g_free(escaped_message);
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
	g_signal_connect(window, "key_press_event",
					  G_CALLBACK(key_press_callback),
					  (gpointer) entry);

	gtk_window_stick(GTK_WINDOW(window));
	gtk_window_set_keep_above(GTK_WINDOW(window), TRUE);
	gtk_widget_grab_focus(entry);	 
	gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	gtk_window_fullscreen(GTK_WINDOW(window));

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
	exit_code = 0;

	gchar *message;

	gtk_init(&argc, &argv);

	if (argc == 2)
		message = argv[1];
	else
		message = "Enter your OpenSSH passphrase:";

	setvbuf(stdout, 0, _IONBF, 0);
	passphrase_dialog(message);
	gtk_main();

	return exit_code;
}
