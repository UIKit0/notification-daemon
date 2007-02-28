#include "config.h"

#include <gtk/gtk.h>
#include <libsexy/sexy-url-label.h>

typedef void (*ActionInvokedCb)(GtkWindow *nw, const char *key);
typedef void (*UrlClickedCb)(GtkWindow *nw, const char *url);

typedef struct
{
	GtkWidget *win;
	GtkWidget *top_spacer;
	GtkWidget *bottom_spacer;
	GtkWidget *main_hbox;
	GtkWidget *iconbox;
	GtkWidget *icon;
	GtkWidget *content_hbox;
	GtkWidget *summary_label;
	GtkWidget *body_label;
	GtkWidget *actions_box;
	GtkWidget *last_sep;
	GtkWidget *stripe_spacer;
	GtkWidget *pie_countdown;

	gboolean has_arrow;
	gboolean enable_transparency;

	int point_x;
	int point_y;

	int drawn_arrow_begin_x;
	int drawn_arrow_begin_y;
	int drawn_arrow_middle_x;
	int drawn_arrow_middle_y;
	int drawn_arrow_end_x;
	int drawn_arrow_end_y;

	int width;
	int height;

	GdkGC *gc;
	GdkPoint *border_points;
	size_t num_border_points;
	GdkRegion *window_region;

	guchar urgency;
	glong timeout;
	glong remaining;

	UrlClickedCb url_clicked;

} WindowData;

enum
{
	URGENCY_LOW,
	URGENCY_NORMAL,
	URGENCY_CRITICAL
};

//#define ENABLE_GRADIENT_LOOK

#ifdef ENABLE_GRADIENT_LOOK
# define STRIPE_WIDTH  45
#else
# define STRIPE_WIDTH  30
#endif

#define WIDTH         400
#define IMAGE_SIZE    32
#define IMAGE_PADDING 10
#define SPACER_LEFT   30
#define PIE_RADIUS    12
#define PIE_WIDTH     (2 * PIE_RADIUS)
#define PIE_HEIGHT    (2 * PIE_RADIUS)
#define BODY_X_OFFSET (IMAGE_SIZE + 8)
#define DEFAULT_ARROW_OFFSET  (SPACER_LEFT + 2)
#define DEFAULT_ARROW_HEIGHT  14
#define DEFAULT_ARROW_WIDTH   28
#define BACKGROUND_OPACITY    0.92
#define BOTTOM_GRADIENT_HEIGHT 30

#if GTK_CHECK_VERSION(2, 8, 0)
# define USE_CAIRO
#endif

#if GTK_CHECK_VERSION(2, 10, 0)
# define USE_COMPOSITE
#endif


#ifdef USE_CAIRO
static void
fill_background(GtkWidget *widget, WindowData *windata, cairo_t *cr)
{
	GtkStyle *style = gtk_widget_get_style(widget);
	GdkColor *background_color = &style->base[GTK_STATE_NORMAL];
#ifdef ENABLE_GRADIENT_LOOK
	cairo_pattern_t *gradient;
	int gradient_y = widget->allocation.height - BOTTOM_GRADIENT_HEIGHT;
#endif

	if (windata->enable_transparency)
	{
		cairo_set_source_rgba(cr,
							  background_color->red   / 65535.0,
							  background_color->green / 65535.0,
							  background_color->blue  / 65535.0,
							  BACKGROUND_OPACITY);
	}
	else
	{
		gdk_cairo_set_source_color(cr, background_color);
	}

	cairo_rectangle(cr, 0, 0,
					widget->allocation.width,
					widget->allocation.height);
	cairo_fill(cr);

#ifdef ENABLE_GRADIENT_LOOK
	/* Add a very subtle gradient to the bottom of the notification */
	gradient = cairo_pattern_create_linear(0, gradient_y, 0,
										   widget->allocation.height);
	cairo_pattern_add_color_stop_rgba(gradient, 0, 0, 0, 0, 0);
	cairo_pattern_add_color_stop_rgba(gradient, 1, 0, 0, 0, 0.15);
	cairo_rectangle(cr, 0, gradient_y, widget->allocation.width,
					BOTTOM_GRADIENT_HEIGHT);
	cairo_set_source(cr, gradient);
	cairo_fill(cr);
	cairo_pattern_destroy(gradient);
#endif
}
#else /* !USE_CAIRO */
static void
fill_background(GtkWidget *widget, WindowData *windata)
{
	GtkStyle *style = gtk_widget_get_style(windata->win);

	gdk_draw_rectangle(GDK_DRAWABLE(widget->window),
					   style->base_gc[GTK_STATE_NORMAL], TRUE,
					   0, 0,
					   widget->allocation.width,
					   widget->allocation.height);
}
#endif

#ifdef USE_CAIRO
static void
draw_stripe(GtkWidget *widget, WindowData *windata, cairo_t *cr)
{
	GtkStyle *style = gtk_widget_get_style(widget);
	GdkColor color;
	int stripe_x = windata->main_hbox->allocation.x + 1;
	int stripe_y = windata->main_hbox->allocation.y + 1;
	int stripe_height = windata->main_hbox->allocation.height - 2;
#ifdef ENABLE_GRADIENT_LOOK
	cairo_pattern_t *gradient;
	double r, g, b;
#endif

	switch (windata->urgency)
	{
		case URGENCY_LOW: // LOW
			color = style->bg[GTK_STATE_NORMAL];
			break;

		case URGENCY_CRITICAL: // CRITICAL
			gdk_color_parse("#CC0000", &color);
			break;

		case URGENCY_NORMAL: // NORMAL
		default:
			color = style->bg[GTK_STATE_SELECTED];
			break;
	}

	cairo_rectangle(cr, stripe_x, stripe_y, STRIPE_WIDTH, stripe_height);

#ifdef ENABLE_GRADIENT_LOOK
	r = color.red   / 65535.0;
	g = color.green / 65535.0;
	b = color.blue  / 65535.0;

	gradient = cairo_pattern_create_linear(stripe_x, 0, STRIPE_WIDTH, 0);
	cairo_pattern_add_color_stop_rgba(gradient, 0, r, g, b, 1);
	cairo_pattern_add_color_stop_rgba(gradient, 1, r, g, b, 0);
	cairo_set_source(cr, gradient);
	cairo_fill(cr);
	cairo_pattern_destroy(gradient);
#else
	gdk_cairo_set_source_color(cr, &color);
	cairo_fill(cr);
#endif
}
#else /* !USE_CAIRO */
static void
draw_stripe(GtkWidget *widget, WindowData *windata)
{
	GtkStyle *style = gtk_widget_get_style(widget);
	gboolean custom_gc = FALSE;
	GdkColor color;
	GdkGC *gc;

	switch (windata->urgency)
	{
		case URGENCY_LOW: // LOW
			gc = style->bg_gc[GTK_STATE_NORMAL];
			break;

		case URGENCY_CRITICAL: // CRITICAL
			custom_gc = TRUE;
			gc = gdk_gc_new(GDK_DRAWABLE(widget->window));
			gdk_color_parse("#CC0000", &color);
			gdk_gc_set_rgb_fg_color(gc, &color);
			break;

		case URGENCY_NORMAL: // NORMAL
		default:
			gc = style->bg_gc[GTK_STATE_SELECTED];
			break;
	}


	gdk_draw_rectangle(widget->window, gc, TRUE,
					   windata->main_hbox->allocation.x + 1,
					   windata->main_hbox->allocation.y + 1,
					   STRIPE_WIDTH,
					   windata->main_hbox->allocation.height - 2);

	if (custom_gc)
		g_object_unref(G_OBJECT(gc));
}
#endif

static GtkArrowType
get_notification_arrow_type(GtkWidget *nw)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	int screen_height;

	screen_height = gdk_screen_get_height(
		gdk_drawable_get_screen(GDK_DRAWABLE(nw->window)));

	if (windata->point_y + windata->height + DEFAULT_ARROW_HEIGHT >
		screen_height)
	{
		return GTK_ARROW_DOWN;
	}
	else
	{
		return GTK_ARROW_UP;
	}
}

#define ADD_POINT(_x, _y, shapeoffset_x, shapeoffset_y) \
	G_STMT_START { \
		windata->border_points[i].x = (_x); \
		windata->border_points[i].y = (_y); \
		shape_points[i].x = (_x) + (shapeoffset_x); \
		shape_points[i].y = (_y) + (shapeoffset_y); \
		i++;\
	} G_STMT_END

static void
create_border_with_arrow(GtkWidget *nw, WindowData *windata)
{
	int width;
	int height;
	int y;
	GtkArrowType arrow_type;
	GdkScreen *screen;
	int screen_width;
	int screen_height;
	int arrow_side1_width = DEFAULT_ARROW_WIDTH / 2;
	int arrow_side2_width = DEFAULT_ARROW_WIDTH / 2;
	int arrow_offset = DEFAULT_ARROW_OFFSET;
	GdkPoint *shape_points = NULL;
	int i = 0;

	width  = windata->width;
	height = windata->height;

	screen        = gdk_drawable_get_screen(GDK_DRAWABLE(nw->window));
	screen_width  = gdk_screen_get_width(screen);
	screen_height = gdk_screen_get_height(screen);

	windata->num_border_points = 5;

	arrow_type = get_notification_arrow_type(windata->win);

	/* Handle the offset and such */
	switch (arrow_type)
	{
		case GTK_ARROW_UP:
		case GTK_ARROW_DOWN:
			if (windata->point_x < arrow_side1_width)
			{
				arrow_side1_width = 0;
				arrow_offset = 0;
			}
			else if (windata->point_x > screen_width - arrow_side2_width)
			{
				arrow_side2_width = 0;
				arrow_offset = width - arrow_side1_width;
			}
			else
			{
				if (windata->point_x - arrow_side2_width + width >=
					screen_width)
				{
					arrow_offset =
						width - arrow_side1_width - arrow_side2_width -
						(screen_width - MAX(windata->point_x +
											arrow_side1_width,
											screen_width -
											DEFAULT_ARROW_OFFSET));
				}
				else
				{
					arrow_offset = MIN(windata->point_x - arrow_side1_width,
									   DEFAULT_ARROW_OFFSET);
				}

				if (arrow_offset == 0 ||
					arrow_offset == width - arrow_side1_width)
					windata->num_border_points++;
				else
					windata->num_border_points += 2;
			}

			/*
			 * Why risk this for official builds? If it's somehow off the
			 * screen, it won't horribly impact the user. Definitely less
			 * than an assertion would...
			 */
#if 0
			g_assert(arrow_offset + arrow_side1_width >= 0);
			g_assert(arrow_offset + arrow_side1_width + arrow_side2_width <=
					 width);
#endif

			windata->border_points = g_new0(GdkPoint,
											windata->num_border_points);
			shape_points = g_new0(GdkPoint, windata->num_border_points);

			windata->drawn_arrow_begin_x = arrow_offset;
			windata->drawn_arrow_middle_x = arrow_offset + arrow_side1_width;
			windata->drawn_arrow_end_x = arrow_offset + arrow_side1_width +
										 arrow_side2_width;

			if (arrow_type == GTK_ARROW_UP)
			{
				windata->drawn_arrow_begin_y = DEFAULT_ARROW_HEIGHT;
				windata->drawn_arrow_middle_y = 0;
				windata->drawn_arrow_end_y = DEFAULT_ARROW_HEIGHT;

				if (arrow_side1_width == 0)
				{
					ADD_POINT(0, 0, 0, 0);
				}
				else
				{
					ADD_POINT(0, DEFAULT_ARROW_HEIGHT, 0, 0);

					if (arrow_offset > 0)
					{
						ADD_POINT(arrow_offset -
								  (arrow_side2_width > 0 ? 0 : 1),
								  DEFAULT_ARROW_HEIGHT, 0, 0);
					}

					ADD_POINT(arrow_offset + arrow_side1_width -
							  (arrow_side2_width > 0 ? 0 : 1),
							  0, 0, 0);
				}

				if (arrow_side2_width > 0)
				{
					ADD_POINT(windata->drawn_arrow_end_x,
							  windata->drawn_arrow_end_y, 1, 0);
					ADD_POINT(width - 1, DEFAULT_ARROW_HEIGHT, 1, 0);
				}

				ADD_POINT(width - 1, height - 1, 1, 1);
				ADD_POINT(0, height - 1, 0, 1);

				y = windata->point_y;
			}
			else
			{
				windata->drawn_arrow_begin_y = height - DEFAULT_ARROW_HEIGHT;
				windata->drawn_arrow_middle_y = height;
				windata->drawn_arrow_end_y = height - DEFAULT_ARROW_HEIGHT;

				ADD_POINT(0, 0, 0, 0);
				ADD_POINT(width - 1, 0, 1, 0);

				if (arrow_side2_width == 0)
				{
					ADD_POINT(width - 1, height,
							  (arrow_side1_width > 0 ? 0 : 1), 0);
				}
				else
				{
					ADD_POINT(width - 1, height - DEFAULT_ARROW_HEIGHT, 1, 1);

					if (arrow_offset < width - arrow_side1_width)
					{
						ADD_POINT(arrow_offset + arrow_side1_width +
								  arrow_side2_width,
								  height - DEFAULT_ARROW_HEIGHT, 0, 1);
					}

					ADD_POINT(arrow_offset + arrow_side1_width, height, 0, 1);
				}

				if (arrow_side1_width > 0)
				{
					ADD_POINT(windata->drawn_arrow_begin_x -
							  (arrow_side2_width > 0 ? 0 : 1),
							  windata->drawn_arrow_begin_y, 0, 0);
					ADD_POINT(0, height - DEFAULT_ARROW_HEIGHT, 0, 1);
				}

				y = windata->point_y - height;
			}

#if 0
			g_assert(i == windata->num_border_points);
			g_assert(windata->point_x - arrow_offset - arrow_side1_width >= 0);
#endif
			gtk_window_move(GTK_WINDOW(windata->win),
							windata->point_x - arrow_offset -
							arrow_side1_width,
							y);

			break;

		case GTK_ARROW_LEFT:
		case GTK_ARROW_RIGHT:
			if (windata->point_y < arrow_side1_width)
			{
				arrow_side1_width = 0;
				arrow_offset = windata->point_y;
			}
			else if (windata->point_y > screen_height - arrow_side2_width)
			{
				arrow_side2_width = 0;
				arrow_offset = windata->point_y - arrow_side1_width;
			}
			break;

		default:
			g_assert_not_reached();
	}

	g_assert(shape_points != NULL);

	windata->window_region =
		gdk_region_polygon(shape_points, windata->num_border_points,
						   GDK_EVEN_ODD_RULE);
	g_free(shape_points);
}

#ifdef USE_CAIRO
static void
draw_border(GtkWidget *widget, WindowData *windata, cairo_t *cr)
{
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1.0);

	if (windata->has_arrow)
	{
		int i;

		create_border_with_arrow(windata->win, windata);

		cairo_move_to(cr,
					  windata->border_points[0].x + 0.5,
					  windata->border_points[0].y + 0.5);

		for (i = 1; i < windata->num_border_points; i++)
		{
			cairo_line_to(cr,
						  windata->border_points[i].x + 0.5,
						  windata->border_points[i].y + 0.5);
		}

		cairo_close_path(cr);
		gdk_window_shape_combine_region(windata->win->window,
										windata->window_region, 0, 0);
		g_free(windata->border_points);
		windata->border_points = NULL;
	}
	else
	{
		cairo_rectangle(cr, 0.5, 0.5,
						windata->width - 0.5, windata->height - 0.5);
	}

	cairo_stroke(cr);
}
#else /* !USE_CAIRO */
static void
draw_border(GtkWidget *widget,
			WindowData *windata)
{
	if (windata->gc == NULL)
	{
		GdkColor color;

		windata->gc = gdk_gc_new(widget->window);
		gdk_color_parse("black", &color);
		gdk_gc_set_rgb_fg_color(windata->gc, &color);
	}

	if (windata->has_arrow)
	{
		create_border_with_arrow(windata->win, windata);

		gdk_draw_polygon(widget->window, windata->gc, FALSE,
						 windata->border_points, windata->num_border_points);
		gdk_window_shape_combine_region(windata->win->window,
										windata->window_region,
										0, 0);
		g_free(windata->border_points);
		windata->border_points = NULL;
	}
	else
	{
		gdk_draw_rectangle(widget->window, windata->gc, FALSE,
						   0, 0, windata->width - 1, windata->height - 1);
	}

}
#endif

static gboolean
paint_window(GtkWidget *widget,
			 GdkEventExpose *event,
			 WindowData *windata)
{
	if (windata->width == 0) {
		windata->width = windata->win->allocation.width;
		windata->height = windata->win->allocation.height;
	}

#ifdef USE_CAIRO
	cairo_t *context;
	cairo_surface_t *surface;
	cairo_t *cr;

	context = gdk_cairo_create(widget->window);

	cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
	surface = cairo_surface_create_similar(cairo_get_target(context),
										   CAIRO_CONTENT_COLOR_ALPHA,
										   widget->allocation.width,
										   widget->allocation.height);
	cr = cairo_create(surface);

	fill_background(widget, windata, cr);
	draw_border(widget, windata, cr);
	draw_stripe(widget, windata, cr);

	cairo_destroy(cr);
	cairo_set_source_surface(context, surface, 0, 0);
	cairo_paint(context);
	cairo_surface_destroy(surface);
	cairo_destroy(context);
#else /* !USE_CAIRO */
	fill_background(widget, windata);
	draw_border(widget, windata);
	draw_stripe(widget, windata);
#endif

	return FALSE;
}

static void
destroy_windata(WindowData *windata)
{
	if (windata->gc != NULL)
		g_object_unref(G_OBJECT(windata->gc));

	if (windata->window_region != NULL)
		gdk_region_destroy(windata->window_region);

	g_free(windata);
}

static void
update_spacers(GtkWidget *nw)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");

	if (windata->has_arrow)
	{
		switch (get_notification_arrow_type(GTK_WIDGET(nw)))
		{
			case GTK_ARROW_UP:
				gtk_widget_show(windata->top_spacer);
				gtk_widget_hide(windata->bottom_spacer);
				break;

			case GTK_ARROW_DOWN:
				gtk_widget_hide(windata->top_spacer);
				gtk_widget_show(windata->bottom_spacer);
				break;

			default:
				g_assert_not_reached();
		}
	}
	else
	{
		gtk_widget_hide(windata->top_spacer);
		gtk_widget_hide(windata->bottom_spacer);
	}
}

static void
update_content_hbox_visibility(WindowData *windata)
{
	/*
	 * This is all a hack, but until we have a libview-style ContentBox,
	 * it'll just have to do.
	 */
	if (GTK_WIDGET_VISIBLE(windata->icon) ||
		GTK_WIDGET_VISIBLE(windata->body_label) ||
		GTK_WIDGET_VISIBLE(windata->actions_box))
	{
		gtk_widget_show(windata->content_hbox);
	}
	else
	{
		gtk_widget_hide(windata->content_hbox);
	}
}

static gboolean
configure_event_cb(GtkWidget *nw,
				   GdkEventConfigure *event,
				   WindowData *windata)
{
	windata->width = event->width;
	windata->height = event->height;

	update_spacers(nw);
	gtk_widget_queue_draw(nw);

	return FALSE;
}

GtkWindow *
create_notification(UrlClickedCb url_clicked)
{
	GtkWidget *spacer;
	GtkWidget *win;
	GtkWidget *drawbox;
	GtkWidget *main_vbox;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *close_button;
	GtkWidget *image;
	GtkWidget *alignment;
	AtkObject *atkobj;
	WindowData *windata;
#ifdef USE_COMPOSITE
	GdkColormap *colormap;
	GdkScreen *screen;
#endif

	windata = g_new0(WindowData, 1);
	windata->urgency = URGENCY_NORMAL;
	windata->url_clicked = url_clicked;

	win = gtk_window_new(GTK_WINDOW_POPUP);
	windata->win = win;

	windata->enable_transparency = FALSE;
#ifdef USE_COMPOSITE
	screen = gtk_window_get_screen(GTK_WINDOW(win));
	colormap = gdk_screen_get_rgba_colormap(screen);

	if (colormap != NULL && gdk_screen_is_composited(screen))
	{
		gtk_widget_set_colormap(win, colormap);
		windata->enable_transparency = TRUE;
	}
#endif

	gtk_window_set_title(GTK_WINDOW(win), "Notification");
	gtk_widget_add_events(win, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);
	gtk_widget_realize(win);
	gtk_widget_set_size_request(win, WIDTH, -1);

	g_object_set_data_full(G_OBJECT(win), "windata", windata,
						   (GDestroyNotify)destroy_windata);
	atk_object_set_role(gtk_widget_get_accessible(win), ATK_ROLE_ALERT);

	g_signal_connect(G_OBJECT(win), "configure_event",
					 G_CALLBACK(configure_event_cb), windata);

	/*
	 * For some reason, there are occasionally graphics glitches when
	 * repainting the window. Despite filling the window with a background
	 * color, parts of the other windows on the screen or the shadows around
	 * notifications will appear on the notification. Somehow, adding this
	 * eventbox makes that problem just go away. Whatever works for now.
	 */
	drawbox = gtk_event_box_new();
	gtk_widget_show(drawbox);
	gtk_container_add(GTK_CONTAINER(win), drawbox);

	main_vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(main_vbox);
	gtk_container_add(GTK_CONTAINER(drawbox), main_vbox);
	gtk_container_set_border_width(GTK_CONTAINER(main_vbox), 1);

	g_signal_connect(G_OBJECT(main_vbox), "expose_event",
					 G_CALLBACK(paint_window), windata);

	windata->top_spacer = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), windata->top_spacer,
					   FALSE, FALSE, 0);
	gtk_widget_set_size_request(windata->top_spacer, -1, DEFAULT_ARROW_HEIGHT);

	windata->main_hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(windata->main_hbox);
	gtk_box_pack_start(GTK_BOX(main_vbox), windata->main_hbox,
					   FALSE, FALSE, 0);

	windata->bottom_spacer = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), windata->bottom_spacer,
					   FALSE, FALSE, 0);
	gtk_widget_set_size_request(windata->bottom_spacer, -1,
								DEFAULT_ARROW_HEIGHT);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_box_pack_start(GTK_BOX(windata->main_hbox), vbox, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	spacer = gtk_image_new();
	gtk_widget_show(spacer);
	gtk_box_pack_start(GTK_BOX(hbox), spacer, FALSE, FALSE, 0);
	gtk_widget_set_size_request(spacer, SPACER_LEFT, -1);

	windata->summary_label = gtk_label_new(NULL);
	gtk_widget_show(windata->summary_label);
	gtk_box_pack_start(GTK_BOX(hbox), windata->summary_label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(windata->summary_label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(windata->summary_label), TRUE);

	atkobj = gtk_widget_get_accessible(windata->summary_label);
	atk_object_set_description(atkobj, "Notification summary text.");

	/* Add the close button */
	close_button = gtk_button_new();
	gtk_widget_show(close_button);
	gtk_box_pack_start(GTK_BOX(hbox), close_button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
	gtk_container_set_border_width(GTK_CONTAINER(close_button), 0);
	gtk_widget_set_size_request(close_button, 20, 20);
	g_signal_connect_swapped(G_OBJECT(close_button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), win);

	atkobj = gtk_widget_get_accessible(close_button);
	atk_action_set_description(ATK_ACTION(atkobj), 0,
							   "Closes the notification.");
	atk_object_set_name(atkobj, "");
	atk_object_set_description(atkobj, "Closes the notification.");

	image = gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
	gtk_widget_show(image);
	gtk_container_add(GTK_CONTAINER(close_button), image);

	windata->content_hbox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), windata->content_hbox, FALSE, FALSE, 0);

	windata->iconbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(windata->iconbox);
	gtk_box_pack_start(GTK_BOX(windata->content_hbox), windata->iconbox,
					   FALSE, FALSE, 0);
	gtk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);

	windata->icon = gtk_image_new();
	gtk_box_pack_start(GTK_BOX(windata->iconbox), windata->icon,
					   TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(windata->icon), 0.5, 0.0);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_box_pack_start(GTK_BOX(windata->content_hbox), vbox, TRUE, TRUE, 0);

	windata->body_label = sexy_url_label_new();
	gtk_box_pack_start(GTK_BOX(vbox), windata->body_label, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(windata->body_label), 0, 0);
	gtk_label_set_line_wrap(GTK_LABEL(windata->body_label), TRUE);
	g_signal_connect_swapped(G_OBJECT(windata->body_label), "url_activated",
							 G_CALLBACK(windata->url_clicked), win);

	atkobj = gtk_widget_get_accessible(windata->body_label);
	atk_object_set_description(atkobj, "Notification body text.");

	alignment = gtk_alignment_new(1, 0.5, 0, 0);
	gtk_widget_show(alignment);
	gtk_box_pack_start(GTK_BOX(vbox), alignment, FALSE, TRUE, 0);

	windata->actions_box = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(alignment), windata->actions_box);

	return GTK_WINDOW(win);
}

void
set_notification_hints(GtkWindow *nw, GHashTable *hints)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	GValue *value;

	g_assert(windata != NULL);

	value = (GValue *)g_hash_table_lookup(hints, "urgency");

	if (value != NULL)
	{
		windata->urgency = g_value_get_uchar(value);

		if (windata->urgency == URGENCY_CRITICAL) {
			gtk_window_set_title(GTK_WINDOW(nw), "Critical Notification");
		} else {
			gtk_window_set_title(GTK_WINDOW(nw), "Notification");
		}
	}
}

void
set_notification_timeout(GtkWindow *nw, glong timeout)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	windata->timeout = timeout;
}

void
notification_tick(GtkWindow *nw, glong remaining)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	windata->remaining = remaining;

	if (windata->pie_countdown != NULL)
	{
		gtk_widget_queue_draw_area(windata->pie_countdown, 0, 0,
								   PIE_WIDTH, PIE_HEIGHT);
	}
}

void
set_notification_text(GtkWindow *nw, const char *summary, const char *body)
{
	char *str;
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	str = g_strdup_printf("<b><big>%s</big></b>", summary);
	gtk_label_set_markup(GTK_LABEL(windata->summary_label), str);
	g_free(str);

	sexy_url_label_set_markup(SEXY_URL_LABEL(windata->body_label), body);

	if (body == NULL || *body == '\0')
		gtk_widget_hide(windata->body_label);
	else
		gtk_widget_show(windata->body_label);

	update_content_hbox_visibility(windata);

	gtk_widget_set_size_request(
		((body != NULL && *body == '\0')
		 ? windata->body_label : windata->summary_label),
		WIDTH - (IMAGE_SIZE + IMAGE_PADDING) - 10,
		-1);
}

void
set_notification_icon(GtkWindow *nw, GdkPixbuf *pixbuf)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(windata->icon), pixbuf);

	if (pixbuf != NULL)
	{
		int pixbuf_width = gdk_pixbuf_get_width(pixbuf);

		gtk_widget_show(windata->icon);
		gtk_widget_set_size_request(windata->iconbox,
									MAX(BODY_X_OFFSET, pixbuf_width), -1);
	}
	else
	{
		gtk_widget_hide(windata->icon);
		gtk_widget_set_size_request(windata->iconbox, BODY_X_OFFSET, -1);
	}

	update_content_hbox_visibility(windata);
}

void
set_notification_arrow(GtkWidget *nw, gboolean visible, int x, int y)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	windata->has_arrow = visible;
	windata->point_x = x;
	windata->point_y = y;

	update_spacers(nw);
}

static gboolean
countdown_expose_cb(GtkWidget *pie, GdkEventExpose *event,
					WindowData *windata)
{
	GtkStyle *style = gtk_widget_get_style(windata->win);

#ifdef USE_CAIRO
	cairo_t *context;
	cairo_surface_t *surface;
	cairo_t *cr;
	context = gdk_cairo_create(GDK_DRAWABLE(windata->pie_countdown->window));
	cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
	surface = cairo_surface_create_similar(
			cairo_get_target(context),
			CAIRO_CONTENT_COLOR_ALPHA,
			pie->allocation.width,
			pie->allocation.height);
	cr = cairo_create(surface);

	fill_background(pie, windata, cr);
#else /* !USE_CAIRO */
	fill_background(pie, windata);
#endif

	if (windata->timeout > 0)
	{
		gdouble pct = (gdouble)windata->remaining / (gdouble)windata->timeout;

#ifdef USE_CAIRO
		gdk_cairo_set_source_color(cr, &style->bg[GTK_STATE_ACTIVE]);

		cairo_move_to(cr, PIE_RADIUS, PIE_RADIUS);
		cairo_arc_negative(cr, PIE_RADIUS, PIE_RADIUS, PIE_RADIUS,
						   -G_PI_2, -(pct * G_PI * 2) - G_PI_2);
		cairo_line_to(cr, PIE_RADIUS, PIE_RADIUS);
		cairo_fill(cr);
#else /* !USE_CAIRO */
		gdk_draw_arc(GDK_DRAWABLE(windata->pie_countdown->window),
					 style->bg_gc[GTK_STATE_ACTIVE], TRUE,
					 0, 0, PIE_WIDTH, PIE_HEIGHT,
					 90 * 64, pct * 360.0 * 64.0);
#endif
	}

#ifdef USE_CAIRO
	cairo_destroy(cr);
	cairo_set_source_surface(context, surface, 0, 0);
	cairo_paint(context);
	cairo_surface_destroy(surface);
	cairo_destroy(context);
#endif

	return TRUE;
}

static void
action_clicked_cb(GtkWidget *w, GdkEventButton *event,
				  ActionInvokedCb action_cb)
{
	GtkWindow *nw   = g_object_get_data(G_OBJECT(w), "_nw");
	const char *key = g_object_get_data(G_OBJECT(w), "_action_key");

	action_cb(nw, key);
}

void
add_notification_action(GtkWindow *nw, const char *text, const char *key,
						ActionInvokedCb cb)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *hbox;
	GdkPixbuf *pixbuf;
	char *buf;

	g_assert(windata != NULL);

	if (!GTK_WIDGET_VISIBLE(windata->actions_box))
	{
		GtkWidget *alignment;

		gtk_widget_show(windata->actions_box);
		update_content_hbox_visibility(windata);

		alignment = gtk_alignment_new(1, 0.5, 0, 0);
		gtk_widget_show(alignment);
		gtk_box_pack_end(GTK_BOX(windata->actions_box), alignment,
						   FALSE, TRUE, 0);

		windata->pie_countdown = gtk_drawing_area_new();
		gtk_widget_show(windata->pie_countdown);
		gtk_container_add(GTK_CONTAINER(alignment), windata->pie_countdown);
		gtk_widget_set_size_request(windata->pie_countdown,
									PIE_WIDTH, PIE_HEIGHT);
		g_signal_connect(G_OBJECT(windata->pie_countdown), "expose_event",
						 G_CALLBACK(countdown_expose_cb), windata);
	}

	button = gtk_button_new();
	gtk_widget_show(button);
	gtk_box_pack_start(GTK_BOX(windata->actions_box), button, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 6);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(button), hbox);

	/* Try to be smart and find a suitable icon. */
	buf = g_strdup_printf("stock_%s", key);
	pixbuf = gtk_icon_theme_load_icon(
		gtk_icon_theme_get_for_screen(
			gdk_drawable_get_screen(GTK_WIDGET(nw)->window)),
		buf, 16, GTK_ICON_LOOKUP_USE_BUILTIN, NULL);
	g_free(buf);

	if (pixbuf != NULL)
	{
		GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_widget_show(image);
		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.5);
	}

	label = gtk_label_new(NULL);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	buf = g_strdup_printf("<small>%s</small>", text);
	gtk_label_set_markup(GTK_LABEL(label), buf);
	g_free(buf);

	g_object_set_data(G_OBJECT(button), "_nw", nw);
	g_object_set_data_full(G_OBJECT(button),
						   "_action_key", g_strdup(key), g_free);
	g_signal_connect(G_OBJECT(button), "button-release-event",
					 G_CALLBACK(action_clicked_cb), cb);
}

void
clear_notification_actions(GtkWindow *nw)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");

	windata->pie_countdown = NULL;

	gtk_widget_hide(windata->actions_box);
	gtk_container_foreach(GTK_CONTAINER(windata->actions_box),
						  (GtkCallback)gtk_object_destroy, NULL);
}

void
move_notification(GtkWidget *nw, int x, int y)
{
	WindowData *windata = g_object_get_data(G_OBJECT(nw), "windata");
	g_assert(windata != NULL);

	if (windata->has_arrow)
	{
		gtk_widget_queue_resize(nw);
	}
	else
	{
		gtk_window_move(GTK_WINDOW(nw), x, y);
	}
}

void
get_theme_info(char **theme_name,
			   char **theme_ver,
			   char **author,
			   char **homepage)
{
	*theme_name = g_strdup("Standard");
	*theme_ver  = g_strdup_printf("%d.%d.%d",
								  NOTIFICATION_DAEMON_MAJOR_VERSION,
								  NOTIFICATION_DAEMON_MINOR_VERSION,
								  NOTIFICATION_DAEMON_MICRO_VERSION);
	*author = g_strdup("Christian Hammond");
	*homepage = g_strdup("http://www.galago-project.org/");
}

gboolean
theme_check_init(unsigned int major_ver, unsigned int minor_ver,
				 unsigned int micro_ver)
{
	return major_ver == NOTIFICATION_DAEMON_MAJOR_VERSION &&
	       minor_ver == NOTIFICATION_DAEMON_MINOR_VERSION &&
	       micro_ver == NOTIFICATION_DAEMON_MICRO_VERSION;
}