#include "../includes.hpp"
#include "../helpers/MiscFunctions.hpp"
#include "ext-workspace-unstable-v1-protocol.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "wlr_ext_workspace_v1.hpp"
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>

#define WORKSPACE_V1_VERSION 1

static void workspace_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) ;

static void workspace_handle_activate(struct wl_client *client,
		struct wl_resource *resource) ;

static void workspace_handle_deactivate(struct wl_client *client,
		struct wl_resource *resource) ;

static void workspace_handle_remove(struct wl_client *client,
		struct wl_resource *resource) ;

static void workspace_group_handle_handle_create_workspace(struct wl_client *client,
		struct wl_resource *resource, const char *arg) ;

static void workspace_group_handle_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) ;

static void workspace_manager_commit(struct wl_client *client,
		struct wl_resource *resource) ;

static void workspace_manager_stop(struct wl_client *client,
		     struct wl_resource *resource) ;

inline static struct zext_workspace_handle_v1_interface workspace_handle_impl = {
    .destroy = workspace_handle_destroy,
    .activate = workspace_handle_activate,
    .deactivate = workspace_handle_deactivate,
    .remove = workspace_handle_remove,
};

inline static struct zext_workspace_group_handle_v1_interface workspace_group_impl = {
    .create_workspace = workspace_group_handle_handle_create_workspace,
    .destroy = workspace_group_handle_handle_destroy,
};

inline static struct zext_workspace_manager_v1_interface workspace_manager_impl = {
    .commit = workspace_manager_commit,
    .stop = workspace_manager_stop,
};

static void workspace_manager_idle_send_done(void *data) {
	struct wlr_ext_workspace_manager_v1 *manager = (wlr_ext_workspace_manager_v1*)data;
	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &manager->resources) {
		zext_workspace_manager_v1_send_done(resource);
	}

	manager->idle_source = NULL;
}

static void workspace_manager_update_idle_source(
		struct wlr_ext_workspace_manager_v1 *manager) {
	if (manager->idle_source) {
		return;
	}

	manager->idle_source = wl_event_loop_add_idle(manager->event_loop,
			workspace_manager_idle_send_done, manager);
}

static struct wlr_ext_workspace_handle_v1 *workspace_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
			&zext_workspace_handle_v1_interface,
			&workspace_handle_impl));
	return (wlr_ext_workspace_handle_v1 *)wl_resource_get_user_data(resource);
}

static struct wlr_ext_workspace_group_handle_v1 *workspace_group_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
			&zext_workspace_group_handle_v1_interface,
			&workspace_group_impl));
	return (wlr_ext_workspace_group_handle_v1 *)wl_resource_get_user_data(resource);
}

static void workspace_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void workspace_handle_activate(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_ext_workspace_handle_v1 *workspace = workspace_from_resource(resource);
	if (!workspace) {
		return;
	}

	workspace->pending |= WLR_EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;
}

static void workspace_handle_remove(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_ext_workspace_handle_v1 *workspace = workspace_from_resource(resource);
	if (!workspace) {
		return;
	}

	wl_signal_emit_mutable(&workspace->events.remove_request, NULL);
}

static void workspace_handle_deactivate(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_ext_workspace_handle_v1 *workspace = workspace_from_resource(resource);
	if (!workspace) {
		return;
	}

	workspace->pending &= ~WLR_EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;
}

static void workspace_handle_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static bool push_entry_in_array(struct wl_array *array, uint32_t entry) {
	uint32_t *index = (uint32_t *)wl_array_add(array, sizeof(uint32_t));
	if (index == NULL) {
		return false;
	}
	*index = entry;
	return true;
}

static bool fill_array_from_workspace_state(struct wl_array *array,
		uint32_t state) {
	if ((state & WLR_EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE) &&
			!push_entry_in_array(array, ZEXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE)) {
		return false;
	}
	if ((state & WLR_EXT_WORKSPACE_HANDLE_V1_STATE_URGENT) &&
			!push_entry_in_array(array, ZEXT_WORKSPACE_HANDLE_V1_STATE_URGENT)) {
		return false;
	}
	if ((state & WLR_EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN) &&
			!push_entry_in_array(array, ZEXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN)) {
		return false;
	}

	return true;
}

static void workspace_handle_send_details_to_resource(
		struct wlr_ext_workspace_handle_v1 *workspace, struct wl_resource *resource) {
	if (workspace->name) {
		zext_workspace_handle_v1_send_name(resource, workspace->name);
	}

	if (workspace->coordinates.size > 0) {
		zext_workspace_handle_v1_send_coordinates(resource,
			&workspace->coordinates);
	}

	struct wl_array state;
	wl_array_init(&state);
	if (!fill_array_from_workspace_state(&state, workspace->server_state)) {
		wl_resource_post_no_memory(resource);
		wl_array_release(&state);
		return;
	}

	zext_workspace_handle_v1_send_state(resource, &state);
}



void wlr_ext_workspace_handle_v1_set_name(struct wlr_ext_workspace_handle_v1 *workspace,
		const char* name) {
	free(workspace->name);
	workspace->name = strdup(name);

	struct wl_resource *tmp, *resource;
	wl_resource_for_each_safe(resource, tmp, &workspace->resources) {
		zext_workspace_handle_v1_send_name(resource, name);
	}

	workspace_manager_update_idle_source(workspace->group->manager);
}

void wlr_ext_workspace_handle_v1_set_coordinates(
		struct wlr_ext_workspace_handle_v1 *workspace, struct wl_array *coordinates) {
	wl_array_copy(&workspace->coordinates, coordinates);

	struct wl_resource *tmp, *resource;
	wl_resource_for_each_safe(resource, tmp, &workspace->resources) {
		zext_workspace_handle_v1_send_coordinates(resource, coordinates);
	}

	workspace_manager_update_idle_source(workspace->group->manager);
}

static void workspace_send_state(struct wlr_ext_workspace_handle_v1 *workspace) {
	struct wl_array state;
	wl_array_init(&state);

	if (!fill_array_from_workspace_state(&state, workspace->server_state)) {
		struct wl_resource *resource;
		wl_resource_for_each(resource, &workspace->resources) {
			wl_resource_post_no_memory(resource);
		}

		wl_array_release(&state);
		return;
	}

	struct wl_resource *tmp, *resource;
	wl_resource_for_each_safe(resource, tmp, &workspace->resources) {
		zext_workspace_handle_v1_send_state(resource, &state);
	}

	wl_array_release(&state);
	workspace_manager_update_idle_source(workspace->group->manager);
}

void wlr_ext_workspace_handle_v1_set_active(
		struct wlr_ext_workspace_handle_v1 *workspace, bool activate) {
	if (activate) {
		workspace->server_state |= WLR_EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;
	} else {
		workspace->server_state &= ~WLR_EXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE;
	}

	workspace_send_state(workspace);
}

void wlr_ext_workspace_handle_v1_set_urgent(
		struct wlr_ext_workspace_handle_v1 *workspace, bool urgent) {
	if (urgent) {
		workspace->server_state |= WLR_EXT_WORKSPACE_HANDLE_V1_STATE_URGENT;
	} else {
		workspace->server_state &= ~WLR_EXT_WORKSPACE_HANDLE_V1_STATE_URGENT;
	}

	workspace_send_state(workspace);
}

void wlr_ext_workspace_handle_v1_set_hidden(
		struct wlr_ext_workspace_handle_v1 *workspace, bool hidden) {
	if (hidden) {
		workspace->server_state |= WLR_EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN;
	} else {
		workspace->server_state &= ~WLR_EXT_WORKSPACE_HANDLE_V1_STATE_HIDDEN;
	}

	workspace_send_state(workspace);
}

static struct wl_resource *create_workspace_resource_for_group_resource(
		struct wlr_ext_workspace_handle_v1 *workspace,
		struct wl_resource *group_resource) {

	struct wl_client *client = wl_resource_get_client(group_resource);
	struct wl_resource *resource = wl_resource_create(client,
			&zext_workspace_handle_v1_interface,
			wl_resource_get_version(group_resource), 0);
	if (!resource) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	wl_resource_set_implementation(resource, &workspace_handle_impl, workspace,
			workspace_handle_resource_destroy);

	wl_list_insert(&workspace->resources, wl_resource_get_link(resource));
	zext_workspace_group_handle_v1_send_workspace(group_resource, resource);

	return resource;
}

struct wlr_ext_workspace_handle_v1 *wlr_ext_workspace_handle_v1_create(
		struct wlr_ext_workspace_group_handle_v1 *group) {
	struct wlr_ext_workspace_handle_v1 *workspace = (wlr_ext_workspace_handle_v1 *)calloc(1,
			sizeof(struct wlr_ext_workspace_handle_v1));
	if (!workspace) {
		return NULL;
	}

	workspace->group = group;
	wl_list_insert(&group->workspaces, &workspace->link);
	wl_array_init(&workspace->coordinates);
	wl_list_init(&workspace->resources);
	wl_signal_init(&workspace->events.remove_request);
	wl_signal_init(&workspace->events.destroy);

	struct wl_resource *tmp, *group_resource;
	wl_resource_for_each_safe(group_resource, tmp, &group->resources) {
		create_workspace_resource_for_group_resource(workspace, group_resource);
	}

	return workspace;
}

void wlr_ext_workspace_handle_v1_destroy(
		struct wlr_ext_workspace_handle_v1 *workspace) {
	if (!workspace) {
		return;
	}

	wl_signal_emit_mutable(&workspace->events.destroy, workspace);

	workspace_manager_update_idle_source(workspace->group->manager);

	struct wl_resource *tmp, *resource;
	wl_resource_for_each_safe(resource, tmp, &workspace->resources) {
		zext_workspace_handle_v1_send_remove(resource);

		wl_resource_set_user_data(resource, NULL);
		wl_list_remove(&resource->link);
		wl_list_init(&resource->link);
	}

	wl_array_release(&workspace->coordinates);
	wl_list_remove(&workspace->link);
	free(workspace->name);
}

static void workspace_group_handle_handle_create_workspace(struct wl_client *client,
		struct wl_resource *resource, const char *arg) {
	struct wlr_ext_workspace_group_handle_v1 *group =
		workspace_group_from_resource(resource);

	struct wlr_ext_workspace_group_handle_v1_create_workspace_event event;
	event.workspace_group = group;
	event.name = arg;
	wl_signal_emit_mutable(&group->events.create_workspace_request, &event);
}

static void workspace_group_handle_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void workspace_group_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

/**
 * Create the workspace group resource and child workspace resources as well.
 */
static struct wl_resource *create_workspace_group_resource_for_resource(
		struct wlr_ext_workspace_group_handle_v1 *group,
		struct wl_resource *manager_resource) {
	struct wl_client *client = wl_resource_get_client(manager_resource);
	struct wl_resource *resource = wl_resource_create(client,
			&zext_workspace_group_handle_v1_interface,
			wl_resource_get_version(manager_resource), 0);
	if (!resource) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	wl_resource_set_implementation(resource, &workspace_group_impl, group,
			workspace_group_resource_destroy);

	wl_list_insert(&group->resources, wl_resource_get_link(resource));
	zext_workspace_manager_v1_send_workspace_group(manager_resource, resource);

	struct wlr_ext_workspace_handle_v1 *tmp, *workspace;
	wl_list_for_each_safe(workspace, tmp, &group->workspaces, link) {
		struct wl_resource *workspace_resource =
			create_workspace_resource_for_group_resource(workspace, resource);
		workspace_handle_send_details_to_resource(workspace, workspace_resource);
	}

	return resource;
}

static void send_output_to_group_resource(struct wl_resource *group_resource,
		struct wlr_output *output, bool enter) {
	struct wl_client *client = wl_resource_get_client(group_resource);
	struct wl_resource *output_resource, *tmp;

	wl_resource_for_each_safe(output_resource, tmp, &output->resources) {
		if (wl_resource_get_client(output_resource) == client) {
			if (enter) {
				zext_workspace_group_handle_v1_send_output_enter(group_resource,
					output_resource);
			} else {
				zext_workspace_group_handle_v1_send_output_leave(group_resource,
					output_resource);
			}
		}
	}
}

static void group_send_output(struct wlr_ext_workspace_group_handle_v1 *group,
		struct wlr_output *output, bool enter) {
	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &group->resources) {
		send_output_to_group_resource(resource, output, enter);
	}
}

static void workspace_handle_output_bind(struct wl_listener *listener,
		void *data) {
	struct wlr_output_event_bind *evt = (wlr_output_event_bind *)data;
	struct wlr_ext_workspace_group_handle_v1_output *output =
		wl_container_of(listener, output, output_bind);
	struct wl_client *client = wl_resource_get_client(evt->resource);

	struct wl_resource *group_resource, *tmp;
	wl_resource_for_each_safe(group_resource, tmp, &output->group_handle->resources) {
		if (client == wl_resource_get_client(group_resource)) {
			zext_workspace_group_handle_v1_send_output_enter(group_resource,
				evt->resource);
		}
	}

	workspace_manager_update_idle_source(output->group_handle->manager);
}

static void workspace_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_ext_workspace_group_handle_v1_output *output =
		wl_container_of(listener, output, output_destroy);
	wlr_ext_workspace_group_handle_v1_output_leave(output->group_handle,
		output->output);
}

void wlr_ext_workspace_group_handle_v1_output_enter(
		struct wlr_ext_workspace_group_handle_v1 *group, struct wlr_output *output) {
	struct wlr_ext_workspace_group_handle_v1_output *group_output;
	wl_list_for_each(group_output, &group->outputs, link) {
		if (group_output->output == output) {
			return; // we have already sent output_enter event
		}
	}

	group_output = (wlr_ext_workspace_group_handle_v1_output *)calloc(1, sizeof(struct wlr_ext_workspace_group_handle_v1_output));
	if (!group_output) {
		wlr_log(WLR_ERROR, "failed to allocate memory for workspace output");
		return;
	}

	group_output->output = output;
	group_output->group_handle = group;
	wl_list_insert(&group->outputs, &group_output->link);

	group_output->output_bind.notify = workspace_handle_output_bind;
	wl_signal_add(&output->events.bind, &group_output->output_bind);

	group_output->output_destroy.notify = workspace_handle_output_destroy;
	wl_signal_add(&output->events.destroy, &group_output->output_destroy);

	group_send_output(group, output, true);
}

static void group_output_destroy(
		struct wlr_ext_workspace_group_handle_v1_output *output) {
	wl_list_remove(&output->link);
	wl_list_remove(&output->output_destroy.link);
	free(output);
}

void wlr_ext_workspace_group_handle_v1_output_leave(
		struct wlr_ext_workspace_group_handle_v1 *group, struct wlr_output *output) {
	struct wlr_ext_workspace_group_handle_v1_output *group_output_iterator;
	struct wlr_ext_workspace_group_handle_v1_output *group_output = NULL;

	wl_list_for_each(group_output_iterator, &group->outputs, link) {
		if (group_output_iterator->output == output) {
			group_output = group_output_iterator;
			break;
		}
	}

	if (group_output) {
		group_send_output(group, output, false);
		group_output_destroy(group_output);
	} else {
		// XXX: log an error? crash?
	}
}

static void group_send_details_to_resource(
		struct wlr_ext_workspace_group_handle_v1 *group,
		struct wl_resource *resource) {
	struct wlr_ext_workspace_group_handle_v1_output *output;
	wl_list_for_each(output, &group->outputs, link) {
		send_output_to_group_resource(resource, output->output, true);
	}
}

struct wlr_ext_workspace_group_handle_v1 *wlr_ext_workspace_group_handle_v1_create(
		struct wlr_ext_workspace_manager_v1 *manager) {

	struct wlr_ext_workspace_group_handle_v1 *group = (wlr_ext_workspace_group_handle_v1 *)calloc(1,
			sizeof(struct wlr_ext_workspace_group_handle_v1));
	if (!group) {
		return NULL;
	}

	group->manager = manager;
	wl_list_insert(&manager->groups, &group->link);

	wl_list_init(&group->outputs);
	wl_list_init(&group->resources);
	wl_list_init(&group->workspaces);
	wl_signal_init(&group->events.create_workspace_request);
	wl_signal_init(&group->events.destroy);

	struct wl_resource *tmp, *manager_resource;
	wl_resource_for_each_safe(manager_resource, tmp, &manager->resources) {
		create_workspace_group_resource_for_resource(group, manager_resource);
	}

	return group;
}

void wlr_ext_workspace_group_handle_v1_destroy(
		struct wlr_ext_workspace_group_handle_v1 *group) {
	if (!group) {
		return;
	}

	struct wlr_ext_workspace_handle_v1 *workspace, *tmp;
	wl_list_for_each_safe(workspace, tmp, &group->workspaces, link) {
		wlr_ext_workspace_handle_v1_destroy(workspace);
	}

	wl_signal_emit_mutable(&group->events.destroy, group);
	workspace_manager_update_idle_source(group->manager);

	struct wlr_ext_workspace_group_handle_v1_output *output, *tmp2;
	wl_list_for_each_safe(output, tmp2, &group->outputs, link) {
		group_output_destroy(output);
	}

	struct wl_resource *tmp3, *resource;
	wl_resource_for_each_safe(resource, tmp3, &group->resources) {
		zext_workspace_group_handle_v1_send_remove(resource);

		wl_resource_set_user_data(resource, NULL);
		wl_list_remove(&resource->link);
		wl_list_init(&resource->link);
	}

	free(group);
}

static struct wlr_ext_workspace_manager_v1 *manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
			&zext_workspace_manager_v1_interface,
			&workspace_manager_impl));
	return (wlr_ext_workspace_manager_v1 *)wl_resource_get_user_data(resource);
}

static void workspace_manager_commit(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_ext_workspace_manager_v1 *manager = manager_from_resource(resource);
	if (!manager) {
		return;
	}

	struct wlr_ext_workspace_group_handle_v1 *group;
	struct wlr_ext_workspace_handle_v1 *workspace;
	wl_list_for_each(group, &manager->groups, link) {
		wl_list_for_each(workspace, &group->workspaces, link) {
			workspace->current = workspace->pending;
		}
	}

	wl_signal_emit_mutable(&manager->events.commit, manager);
}

static void workspace_manager_stop(struct wl_client *client,
		     struct wl_resource *resource) {
	struct wlr_ext_workspace_manager_v1 *manager = manager_from_resource(resource);
	if (!manager) {
		return;
	}

	zext_workspace_manager_v1_send_finished(resource);
	wl_resource_destroy(resource);
}

static void workspace_manager_resource_destroy( struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static void workspace_manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_ext_workspace_manager_v1 *manager = (wlr_ext_workspace_manager_v1 *)data;
	struct wl_resource *resource = wl_resource_create(client,
			&zext_workspace_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &workspace_manager_impl,
			manager, workspace_manager_resource_destroy);

	wl_list_insert(&manager->resources, wl_resource_get_link(resource));

	struct wlr_ext_workspace_group_handle_v1 *group, *tmp;
	wl_list_for_each_safe(group, tmp, &manager->groups, link) {
		struct wl_resource *group_resource =
			create_workspace_group_resource_for_resource(group, resource);
		group_send_details_to_resource(group, group_resource);
	}

	zext_workspace_manager_v1_send_done(resource);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_ext_workspace_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_signal_emit_mutable(&manager->events.destroy, manager);
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);

	free(manager);
}

struct wlr_ext_workspace_manager_v1 *wlr_ext_workspace_manager_v1_create(
		struct wl_display *display) {

	struct wlr_ext_workspace_manager_v1 *manager = (wlr_ext_workspace_manager_v1 *)calloc(1,
			sizeof(struct wlr_ext_workspace_manager_v1));
	if (!manager) {
		return NULL;
	}

	manager->event_loop = wl_display_get_event_loop(display);
	manager->global = wl_global_create(display,
			&zext_workspace_manager_v1_interface,
			WORKSPACE_V1_VERSION, manager,
			workspace_manager_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.destroy);
	wl_signal_init(&manager->events.commit);
	wl_list_init(&manager->resources);
	wl_list_init(&manager->groups);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}
