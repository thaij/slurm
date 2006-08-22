/*****************************************************************************\
 *  part_info.c - Functions related to partition display 
 *  mode of sview.
 *****************************************************************************
 *  Copyright (C) 2004-2006 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Danny Auble <da@llnl.gov>
 *
 *  UCRL-CODE-217948.
 *  
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.llnl.gov/linux/slurm/>.
 *  
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

#include "src/sview/sview.h"

#define _DEBUG 0
DEF_TIMERS;

enum { 
	SORTID_POS = POS_LOC,
	SORTID_NAME, 
	SORTID_DEFAULT, 
	SORTID_AVAIL, 
	SORTID_TIMELIMIT, 
	SORTID_JOB_SIZE,
	SORTID_ROOT, 
	SORTID_SHARE, 
	SORTID_GROUPS,
	SORTID_NODES, 
	SORTID_CPUS, 
	SORTID_DISK, 
	SORTID_NODELIST, 
	SORTID_STATE,
	SORTID_UPDATED, 
	SORTID_CNT
};

static display_data_t display_data_part[] = {
	{G_TYPE_INT, SORTID_POS, NULL, FALSE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_NAME, "Partition", TRUE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_DEFAULT, "Default", TRUE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_AVAIL, "Availablity", TRUE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_TIMELIMIT, "Time Limit", 
	 TRUE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_JOB_SIZE, "Job Size", FALSE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_ROOT, "Root", FALSE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_SHARE, "Share", FALSE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_GROUPS, "Groups", FALSE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_NODES, "Nodes", TRUE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_CPUS, "CPUs", FALSE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_DISK, "Disk", FALSE, -1, refresh_part},
	{G_TYPE_STRING, SORTID_STATE, "State", TRUE, -1, refresh_part},
#ifdef HAVE_BG
	{G_TYPE_STRING, SORTID_NODELIST, "BP List", TRUE, -1, refresh_part},
#else
	{G_TYPE_STRING, SORTID_NODELIST, "NodeList", TRUE, -1, refresh_part},
#endif
	{G_TYPE_INT, SORTID_UPDATED, NULL, FALSE, -1, refresh_part},

	{G_TYPE_NONE, -1, NULL, FALSE, -1}
};

static display_data_t options_data_part[] = {
	{G_TYPE_INT, SORTID_POS, NULL, FALSE, -1},
	{G_TYPE_STRING, INFO_PAGE, "Full Info", TRUE, PART_PAGE},
	{G_TYPE_STRING, JOB_PAGE, "Jobs", TRUE, PART_PAGE},
#ifdef HAVE_BG
	{G_TYPE_STRING, BLOCK_PAGE, "Blocks", TRUE, PART_PAGE},
	{G_TYPE_STRING, NODE_PAGE, "Base Partitions", TRUE, PART_PAGE},
#else
	{G_TYPE_STRING, NODE_PAGE, "Nodes", TRUE, PART_PAGE},
#endif
	{G_TYPE_STRING, SUBMIT_PAGE, "Job Submit", TRUE, PART_PAGE},
	{G_TYPE_STRING, ADMIN_PAGE, "Admin", TRUE, PART_PAGE},
	{G_TYPE_NONE, -1, NULL, FALSE, -1}
};

static display_data_t *local_display_data = NULL;

static int 
_build_min_max_string(char *buffer, int buf_size, int min, int max, bool range)
{
	char tmp_min[7];
	char tmp_max[7];
	convert_to_kilo(min, tmp_min);
	convert_to_kilo(max, tmp_max);
	
	if (max == min)
		return snprintf(buffer, buf_size, "%s", tmp_max);
	else if (range) {
		if (max == INFINITE)
			return snprintf(buffer, buf_size, "%s-infinite", 
					tmp_min);
		else
			return snprintf(buffer, buf_size, "%s-%s", 
					tmp_min, tmp_max);
	} else
		return snprintf(buffer, buf_size, "%s+", tmp_min);
}

static void _update_part_record(partition_info_t *part_ptr,
				GtkTreeStore *treestore, GtkTreeIter *iter)
{
	char time_buf[20];
	char tmp_cnt[7];

	gtk_tree_store_set(treestore, iter, SORTID_NAME, part_ptr->name, -1);

	if(part_ptr->default_part)
		gtk_tree_store_set(treestore, iter, SORTID_DEFAULT, "*", -1);
	
	if (part_ptr->state_up) 
		gtk_tree_store_set(treestore, iter, SORTID_AVAIL, "up", -1);
	else
		gtk_tree_store_set(treestore, iter, SORTID_AVAIL, "down", -1);
		
	if (part_ptr->max_time == INFINITE)
		snprintf(time_buf, sizeof(time_buf), "infinite");
	else {
		snprint_time(time_buf, sizeof(time_buf), 
			     (part_ptr->max_time * 60));
	}
	
	gtk_tree_store_set(treestore, iter, SORTID_TIMELIMIT, time_buf, -1);
	
	_build_min_max_string(time_buf, sizeof(time_buf), 
			      part_ptr->min_nodes, 
			      part_ptr->max_nodes, true);
	gtk_tree_store_set(treestore, iter, SORTID_JOB_SIZE, time_buf, -1);

	if(part_ptr->root_only)
		gtk_tree_store_set(treestore, iter, SORTID_ROOT, "yes", -1);
	else
		gtk_tree_store_set(treestore, iter, SORTID_ROOT, "no", -1);
	
	if(part_ptr->shared > 1)
		gtk_tree_store_set(treestore, iter, SORTID_SHARE, "force", -1);
	else if(part_ptr->shared)
		gtk_tree_store_set(treestore, iter, SORTID_SHARE, "yes", -1);
	else
		gtk_tree_store_set(treestore, iter, SORTID_SHARE, "no", -1);
	
	if(part_ptr->allow_groups)
		gtk_tree_store_set(treestore, iter, SORTID_GROUPS,
				   part_ptr->allow_groups, -1);
	else
		gtk_tree_store_set(treestore, iter, SORTID_GROUPS, "all", -1);

	convert_to_kilo(part_ptr->total_nodes, tmp_cnt);
	gtk_tree_store_set(treestore, iter, SORTID_NODES, tmp_cnt, -1);

	gtk_tree_store_set(treestore, iter, SORTID_NODELIST, 
			   part_ptr->nodes, -1);
	gtk_tree_store_set(treestore, iter, SORTID_UPDATED, 1, -1);	

	return;
}
static void _append_part_record(partition_info_t *part_ptr,
				GtkTreeStore *treestore, GtkTreeIter *iter,
				int line)
{
	gtk_tree_store_append(treestore, iter, NULL);
	gtk_tree_store_set(treestore, iter, SORTID_POS, line, -1);
	_update_part_record(part_ptr, treestore, iter);
}

static void _update_info_part(partition_info_msg_t *part_info_ptr, 
			      GtkTreeView *tree_view,
			      specific_info_t *spec_info)
{
	GtkTreePath *path = gtk_tree_path_new_first();
	GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
	GtkTreeIter iter;
	int i;
	partition_info_t part;
	int line = 0;
	char *host = NULL, *host2 = NULL, *part_name = NULL;
	hostlist_t hostlist = NULL;
	int found = 0;

	if(spec_info) {
		switch(spec_info->type) {
		case NODE_PAGE:
			hostlist = hostlist_create((char *)spec_info->data);
			host = hostlist_shift(hostlist);
			hostlist_destroy(hostlist);
			if(host == NULL) {
				g_print("nodelist was empty");
				return;
			}		
			break;
		}
	}
 
	/* get the iter, or find out the list is empty goto add */
	if (gtk_tree_model_get_iter(model, &iter, path)) {
		/* make sure all the partitions are still here */
		while(1) {
			gtk_tree_store_set(GTK_TREE_STORE(model), &iter, 
					   SORTID_UPDATED, 0, -1);	
			if(!gtk_tree_model_iter_next(model, &iter)) {
				break;
			}
		}
	}

	for (i = 0; i < part_info_ptr->record_count; i++) {
		part = part_info_ptr->partition_array[i];
		if (!part.nodes || (part.nodes[0] == '\0'))
			continue;	/* empty partition */
		/* get the iter, or find out the list is empty goto add */
		if (!gtk_tree_model_get_iter(model, &iter, path)) {
			goto adding;
		}
		while(1) {
			/* search for the jobid and check to see if 
			   it is in the list */
			gtk_tree_model_get(model, &iter, SORTID_NAME, 
					   &part_name, -1);
			if(!strcmp(part_name, part.name)) {
				/* update with new info */
				g_free(part_name);
				_update_part_record(&part, 
						    GTK_TREE_STORE(model), 
						    &iter);
				goto found;
			}
			g_free(part_name);
				
			/* see what line we were on to add the next one 
			   to the list */
			gtk_tree_model_get(model, &iter, SORTID_POS, 
					   &line, -1);
			if(!gtk_tree_model_iter_next(model, &iter)) {
				line++;
				break;
			}
		}
	adding:
		if(spec_info) {
			switch(spec_info->type) {
			case NODE_PAGE:
				if(!part.nodes || !host)
					continue;
				
				hostlist = hostlist_create(part.nodes);	
				found = 0;
				while((host2 = hostlist_shift(hostlist))) { 
					if(!strcmp(host, host2)) {
						free(host2);
						found = 1;
						break; 
					}
					free(host2);
				}
				hostlist_destroy(hostlist);
				if(!found)
					continue;
				break;
			case BLOCK_PAGE:
			case JOB_PAGE:
				if(strcmp(part.name, (char *)spec_info->data)) 
					continue;
				break;
			default:
				g_print("Unkown type %d\n", spec_info->type);
				continue;
			}
		}
		_append_part_record(&part, GTK_TREE_STORE(model), 
				    &iter, line);
	found:
		;
	}
	if(host)
		free(host);

	gtk_tree_path_free(path);
	/* remove all old partitions */
	remove_old(model, SORTID_UPDATED);
}

void _display_info_part(partition_info_msg_t *part_info_ptr,
			popup_info_t *popup_win)
{
	specific_info_t *spec_info = popup_win->spec_info;
	char *name = (char *)spec_info->data;
	int i, found = 0;
	partition_info_t part;
	char *info = NULL;
	char *not_found = NULL;
	GtkWidget *label = NULL;
	
	if(!spec_info->data) {
		info = xstrdup("No pointer given!");
		goto finished;
	}

	if(spec_info->display_widget) {
		not_found = 
			xstrdup(GTK_LABEL(spec_info->display_widget)->text);
		gtk_widget_destroy(spec_info->display_widget);
		spec_info->display_widget = NULL;
	}
	for (i = 0; i < part_info_ptr->record_count; i++) {
		part = part_info_ptr->partition_array[i];
		if (!part.nodes || (part.nodes[0] == '\0'))
			continue;	/* empty partition */
		if(!strcmp(part.name, name)) {
			if(!(info = slurm_sprint_partition_info(&part, 0))) {
				info = xmalloc(100);
				sprintf(info, 
					"Problem getting partition "
					"info for %s", 
					part.name);
			}
			found = 1;
			break;
		}
	}
	if(!found) {
		char *temp = "PARTITION DOESN'T EXSIST\n";
		if(!not_found || strncmp(temp, not_found, strlen(temp))) 
			info = xstrdup(temp);
		xstrcat(info, not_found);
	}
finished:
	label = gtk_label_new(info);
	xfree(info);
	xfree(not_found);
	gtk_table_attach_defaults(popup_win->table, label, 0, 1, 0, 1); 
	gtk_widget_show(label);	
	spec_info->display_widget = gtk_widget_ref(GTK_WIDGET(label));
	
	return;
}

void *_popup_thr_part(void *arg)
{
	popup_thr(arg);		
	return NULL;
}

extern void refresh_part(GtkAction *action, gpointer user_data)
{
	popup_info_t *popup_win = (popup_info_t *)user_data;
	xassert(popup_win != NULL);
	xassert(popup_win->spec_info != NULL);
	xassert(popup_win->spec_info->title != NULL);
	popup_win->force_refresh = 1;
	specific_info_part(popup_win);
}

extern int get_new_info_part(partition_info_msg_t **part_ptr, int force)
{
	static partition_info_msg_t *part_info_ptr = NULL;
	static partition_info_msg_t *new_part_ptr = NULL;
	int error_code = SLURM_SUCCESS;
	time_t now = time(NULL);
	static time_t last;
		
	if(!force && ((now - last) < global_sleep_time)) {
		*part_ptr = part_info_ptr;
		return error_code;
	}
	last = now;
	if (part_info_ptr) {
		error_code = slurm_load_partitions(part_info_ptr->last_update, 
						   &new_part_ptr, SHOW_ALL);
		if (error_code == SLURM_SUCCESS)
			slurm_free_partition_info_msg(part_info_ptr);
		else if (slurm_get_errno() == SLURM_NO_CHANGE_IN_DATA) {
			error_code = SLURM_NO_CHANGE_IN_DATA;
			new_part_ptr = part_info_ptr;
		}
	} else {
		error_code = slurm_load_partitions((time_t) NULL, 
						   &new_part_ptr, SHOW_ALL);
	}
	
	part_info_ptr = new_part_ptr;
	*part_ptr = new_part_ptr;
	return error_code;
}

extern void get_info_part(GtkTable *table, display_data_t *display_data)
{
	int error_code = SLURM_SUCCESS;
	static int view = -1;
	static partition_info_msg_t *part_info_ptr = NULL;
	char error_char[100];
	GtkWidget *label = NULL;
	GtkTreeView *tree_view = NULL;
	static GtkWidget *display_widget = NULL;

	if(display_data)
		local_display_data = display_data;
	if(!table) {
		display_data_part->set_menu = local_display_data->set_menu;
		return;
	}
	if(part_info_ptr && toggled) {
		gtk_widget_destroy(display_widget);
		display_widget = NULL;
		goto display_it;
	}
	
	if((error_code = get_new_info_part(&part_info_ptr, force_refresh))
	   == SLURM_NO_CHANGE_IN_DATA) { 
		if(!display_widget || view == ERROR_VIEW)
			goto display_it;
		goto update_it;
	}
	
	if (error_code != SLURM_SUCCESS) {
		if(view == ERROR_VIEW)
			goto end_it;
		if(display_widget)
			gtk_widget_destroy(display_widget);
		view = ERROR_VIEW;
		snprintf(error_char, 100, "slurm_load_partitions: %s",
			slurm_strerror(slurm_get_errno()));
		label = gtk_label_new(error_char);
		display_widget = gtk_widget_ref(GTK_WIDGET(label));
		gtk_table_attach_defaults(table, label, 0, 1, 0, 1);
		gtk_widget_show(label);
		goto end_it;
	}
display_it:	
	if(view == ERROR_VIEW && display_widget) {
		gtk_widget_destroy(display_widget);
		display_widget = NULL;
	}
	if(!display_widget) {
		tree_view = create_treeview(local_display_data, part_info_ptr);

		display_widget = gtk_widget_ref(GTK_WIDGET(tree_view));
		gtk_table_attach_defaults(table,
					  GTK_WIDGET(tree_view),
					  0, 1, 0, 1);
		gtk_widget_show(GTK_WIDGET(tree_view));
		/* since this function sets the model of the tree_view 
		   to the treestore we don't really care about 
		   the return value */
		create_treestore(tree_view, display_data_part, SORTID_CNT);
	}
update_it:
	view = INFO_VIEW;
	_update_info_part(part_info_ptr, GTK_TREE_VIEW(display_widget), NULL);
end_it:
	toggled = FALSE;
	force_refresh = 0;
	
	return;
}

extern void specific_info_part(popup_info_t *popup_win)
{
	int error_code = SLURM_SUCCESS;
	static partition_info_msg_t *part_info_ptr = NULL;
	specific_info_t *spec_info = popup_win->spec_info;
	char error_char[100];
	GtkWidget *label = NULL;
	GtkTreeView *tree_view = NULL;
	
	if(!spec_info->display_widget)
		setup_popup_info(popup_win, display_data_part, SORTID_CNT);
	
	if(part_info_ptr && popup_win->toggled) {
		gtk_widget_destroy(spec_info->display_widget);
		spec_info->display_widget = NULL;
		goto display_it;
	}

	if((error_code = get_new_info_part(&part_info_ptr, 
					   popup_win->force_refresh))
	   == SLURM_NO_CHANGE_IN_DATA)  {
		if(!spec_info->display_widget || spec_info->view == ERROR_VIEW)
			goto display_it;
		
		goto update_it;
	}
		
	if (error_code != SLURM_SUCCESS) {
		if(spec_info->view == ERROR_VIEW)
			goto end_it;
		if(spec_info->display_widget) {
			gtk_widget_destroy(spec_info->display_widget);
			spec_info->display_widget = NULL;
		}
		spec_info->view = ERROR_VIEW;
		snprintf(error_char, 100, "slurm_load_partitions: %s",
			 slurm_strerror(slurm_get_errno()));
		label = gtk_label_new(error_char);
		gtk_table_attach_defaults(popup_win->table, 
					  label,
					  0, 1, 0, 1); 
		gtk_widget_show(label);			
		spec_info->display_widget = gtk_widget_ref(GTK_WIDGET(label));
		goto end_it;
	}
display_it:	
			
	if(spec_info->view == ERROR_VIEW && spec_info->display_widget) {
		gtk_widget_destroy(spec_info->display_widget);
		spec_info->display_widget = NULL;
	}
	
	if(spec_info->type != INFO_PAGE && !spec_info->display_widget) {
		tree_view = create_treeview(local_display_data, part_info_ptr);
		
		spec_info->display_widget = 
			gtk_widget_ref(GTK_WIDGET(tree_view));
		gtk_table_attach_defaults(popup_win->table,
					  GTK_WIDGET(tree_view),
					  0, 1, 0, 1);
		
		/* since this function sets the model of the tree_view 
		   to the treestore we don't really care about 
		   the return value */
		create_treestore(tree_view, popup_win->display_data, 
				 SORTID_CNT);
	}
update_it:
	spec_info->view = INFO_VIEW;
	if(spec_info->type == INFO_PAGE) {
		_display_info_part(part_info_ptr, popup_win);
	} else {
		_update_info_part(part_info_ptr, 
				  GTK_TREE_VIEW(spec_info->display_widget), 
				  spec_info);
	}
	
end_it:
	popup_win->toggled = 0;
	popup_win->force_refresh = 0;
		
	return;
}

extern void set_menus_part(void *arg, GtkTreePath *path, 
			   GtkMenu *menu, int type)
{
	GtkTreeView *tree_view = (GtkTreeView *)arg;
	popup_info_t *popup_win = (popup_info_t *)arg;
	switch(type) {
	case TAB_CLICKED:
		make_fields_menu(menu, display_data_part);
		break;
	case ROW_CLICKED:
		make_options_menu(tree_view, path, menu, options_data_part);
		break;
	case POPUP_CLICKED:
		make_popup_fields_menu(popup_win, menu);
		break;
	default:
		g_error("UNKNOWN type %d given to set_fields\n", type);
	}
}

extern void popup_all_part(GtkTreeModel *model, GtkTreeIter *iter, int id)
{
	char *name = NULL;
	char title[100];
	ListIterator itr = NULL;
	popup_info_t *popup_win = NULL;
	GError *error = NULL;
					
	gtk_tree_model_get(model, iter, SORTID_NAME, &name, -1);
	
	switch(id) {
	case JOB_PAGE:
		snprintf(title, 100, "Job(s) in partition %s", name);
		break;
	case NODE_PAGE:
#ifdef HAVE_BG
		snprintf(title, 100, 
			 "Base partition(s) in partition %s", name);
#else
		snprintf(title, 100, "Node(s) in partition %s", name);
#endif
		break;
	case BLOCK_PAGE: 
		snprintf(title, 100, "Block(s) in partition %s", name);
		break;
	case ADMIN_PAGE: 
		snprintf(title, 100, "Admin page for partition %s", name);
		break;
	case SUBMIT_PAGE: 
		snprintf(title, 100, "Submit job in partition %s", name);
		break;
	case INFO_PAGE: 
		snprintf(title, 100, "Full info for partition %s", name);
		break;
	default:
		g_print("part got %d\n", id);
	}
	
	itr = list_iterator_create(popup_list);
	while((popup_win = list_next(itr))) {
		if(popup_win->spec_info)
			if(!strcmp(popup_win->spec_info->title, title)) {
				break;
			} 
	}
	list_iterator_destroy(itr);

	if(!popup_win) {
		if(id == INFO_PAGE)
			popup_win = create_popup_info(id, PART_PAGE, title);
		else
			popup_win = create_popup_info(PART_PAGE, id, title);
	}
	switch(id) {
	case JOB_PAGE:
		popup_win->spec_info->data = name;
		//specific_info_job(popup_win);
		break;
	case NODE_PAGE:
		g_free(name);
		gtk_tree_model_get(model, iter, SORTID_NODELIST, &name, -1);
		popup_win->spec_info->data = name;
		//specific_info_node(popup_win);
		break;
	case BLOCK_PAGE: 
		popup_win->spec_info->data = name;
		break;
	case ADMIN_PAGE: 
		break;
	case SUBMIT_PAGE: 
		break;
	case INFO_PAGE:
		popup_win->spec_info->data = name;
		break;
	default:
		g_print("part got %d\n", id);
	}
	if (!g_thread_create(_popup_thr_part, popup_win, FALSE, &error))
	{
		g_printerr ("Failed to create part popup thread: %s\n", 
			    error->message);
		return;
	}		
}
