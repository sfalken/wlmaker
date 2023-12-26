/* ========================================================================= */
/**
 * @file surface.c
 *
 * @copyright
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "surface.h"

#include "element.h"
#include "util.h"

#define WLR_USE_UNSTABLE
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_seat.h>
#undef WLR_USE_UNSTABLE

/* == Declarations ========================================================= */

static void _wlmtk_surface_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static void _wlmtk_surface_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);
static void _wlmtk_surface_element_pointer_leave(wlmtk_element_t *element_ptr);
static bool _wlmtk_surface_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    uint32_t time_msec);
static bool _wlmtk_surface_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr);

static void handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr);

/* == Data ================================================================= */

/** Method table for the element's virtual methods. */
static const wlmtk_element_vmt_t surface_element_vmt = {
    .get_dimensions = _wlmtk_surface_element_get_dimensions,
    .get_pointer_area = _wlmtk_surface_element_get_pointer_area,
    .pointer_leave = _wlmtk_surface_element_pointer_leave,
    .pointer_motion = _wlmtk_surface_element_pointer_motion,
    .pointer_button = _wlmtk_surface_element_pointer_button,
};

/* == Exported methods ===================================================== */

/* ------------------------------------------------------------------------- */
bool wlmtk_surface_init(
    wlmtk_surface_t *surface_ptr,
    struct wlr_surface *wlr_surface_ptr,
    wlmtk_env_t *env_ptr)
{
    BS_ASSERT(NULL != surface_ptr);
    memset(surface_ptr, 0, sizeof(wlmtk_surface_t));

    if (!wlmtk_element_init(&surface_ptr->super_element, env_ptr)) {
        wlmtk_surface_fini(surface_ptr);
        return false;
    }
    surface_ptr->orig_super_element_vmt = wlmtk_element_extend(
        &surface_ptr->super_element, &surface_element_vmt);

    surface_ptr->wlr_surface_ptr = wlr_surface_ptr;
    if (NULL != surface_ptr->wlr_surface_ptr) {
        wlmtk_util_connect_listener_signal(
            &surface_ptr->wlr_surface_ptr->events.commit,
            &surface_ptr->surface_commit_listener,
            handle_surface_commit);
    }
    return true;
}

/* ------------------------------------------------------------------------- */
void wlmtk_surface_fini(wlmtk_surface_t *surface_ptr)
{
    if (NULL != surface_ptr->wlr_surface_ptr) {
        wl_list_remove(&surface_ptr->surface_commit_listener.link);
        surface_ptr->wlr_surface_ptr= NULL;
    }

    wlmtk_element_fini(&surface_ptr->super_element);
    memset(surface_ptr, 0, sizeof(wlmtk_surface_t));
}

/* ------------------------------------------------------------------------- */
wlmtk_surface_vmt_t wlmtk_surface_extend(
    wlmtk_surface_t *surface_ptr,
    const wlmtk_surface_vmt_t *surface_vmt_ptr)
{
    wlmtk_surface_vmt_t orig_vmt = surface_ptr->vmt;

    if (NULL != surface_vmt_ptr->request_size) {
        surface_ptr->vmt.request_size = surface_vmt_ptr->request_size;
    }

    return orig_vmt;
}

/* ------------------------------------------------------------------------- */
wlmtk_element_t *wlmtk_surface_element(wlmtk_surface_t *surface_ptr)
{
    return &surface_ptr->super_element;
}

/* ------------------------------------------------------------------------- */
void wlmtk_surface_get_size(
    wlmtk_surface_t *surface_ptr,
    int *width_ptr,
    int *height_ptr)
{
    if (NULL != width_ptr) *width_ptr = surface_ptr->committed_width;
    if (NULL != height_ptr) *height_ptr = surface_ptr->committed_height;
}

/* == Local (static) methods =============================================== */

/* ------------------------------------------------------------------------- */
/**
 * Implementation of the element's get_dimensions method: Return dimensions.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void _wlmtk_surface_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_surface_t *surface_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_surface_t, super_element);

    struct wlr_box box;
    wlr_surface_get_extends(surface_ptr->wlr_surface_ptr, &box);
    if (NULL != left_ptr) *left_ptr = box.x;
    if (NULL != top_ptr) *top_ptr = box.y;
    if (NULL != right_ptr) *right_ptr = box.width;
    if (NULL != bottom_ptr) *bottom_ptr = box.height;
}

/* ------------------------------------------------------------------------- */
/**
 * Overwrites the element's get_pointer_area method: Returns the extents of
 * the surface and all subsurfaces.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void _wlmtk_surface_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr)
{
    wlmtk_surface_t *surface_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_surface_t, super_element);

    struct wlr_box box;
    wlr_surface_get_extends(surface_ptr->wlr_surface_ptr, &box);

    if (NULL != left_ptr) *left_ptr = box.x;
    if (NULL != top_ptr) *top_ptr = box.y;
    if (NULL != right_ptr) *right_ptr = box.width - box.x;
    if (NULL != bottom_ptr) *bottom_ptr = box.height - box.y;
}

/* ------------------------------------------------------------------------- */
/**
 * Implements the element's leave method: If there's a WLR (sub)surface
 * currently holding focus, that will be cleared.
 *
 * @param element_ptr
 */
void _wlmtk_surface_element_pointer_leave(wlmtk_element_t *element_ptr)
{
    wlmtk_surface_t *surface_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_surface_t, super_element);

    // If the current surface's parent is our surface: clear it.
    struct wlr_surface *focused_wlr_surface_ptr =
        wlmtk_env_wlr_seat(surface_ptr->super_element.env_ptr
            )->pointer_state.focused_surface;
    if (NULL != focused_wlr_surface_ptr &&
        wlr_surface_get_root_surface(focused_wlr_surface_ptr) ==
        surface_ptr->wlr_surface_ptr) {
        wlr_seat_pointer_clear_focus(
            wlmtk_env_wlr_seat(surface_ptr->super_element.env_ptr));
    }
}

/* ------------------------------------------------------------------------- */
/**
 * Pass pointer motion events to client's surface.
 *
 * Identifies the surface (or sub-surface) at the given coordinates, and pass
 * on the motion event to that surface. If needed, will update the seat's
 * pointer focus.
 *
 * @param element_ptr
 * @param x                   Pointer horizontal position, relative to this
 *                            element's node.
 * @param y                   Pointer vertical position, relative to this
 *                            element's node.
 * @param time_msec
 *
 * @return Whether if the motion is within the area.
 */
bool _wlmtk_surface_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    uint32_t time_msec)
{
    wlmtk_surface_t *surface_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_surface_t, super_element);

    surface_ptr->orig_super_element_vmt.pointer_motion(
        element_ptr, x, y, time_msec);

    if (NULL == surface_ptr->super_element.wlr_scene_node_ptr) return false;

    // Get the layout local coordinates of the node, so we can adjust the
    // node-local (x, y) for the `wlr_scene_node_at` call.
    int lx, ly;
    if (!wlr_scene_node_coords(
            surface_ptr->super_element.wlr_scene_node_ptr, &lx, &ly)) {
        return false;
    }
    // Get the node below the cursor. Return if there's no buffer node.
    double node_x, node_y;
    struct wlr_scene_node *wlr_scene_node_ptr = wlr_scene_node_at(
        surface_ptr->super_element.wlr_scene_node_ptr,
        x + lx, y + ly, &node_x, &node_y);

    if (NULL == wlr_scene_node_ptr ||
        WLR_SCENE_NODE_BUFFER != wlr_scene_node_ptr->type) {
        return false;
    }

    struct wlr_scene_buffer *wlr_scene_buffer_ptr =
        wlr_scene_buffer_from_node(wlr_scene_node_ptr);
    struct wlr_scene_surface *wlr_scene_surface_ptr =
        wlr_scene_surface_try_from_buffer(wlr_scene_buffer_ptr);
    if (NULL == wlr_scene_surface_ptr) {
        return false;
    }

    BS_ASSERT(surface_ptr->wlr_surface_ptr ==
              wlr_surface_get_root_surface(wlr_scene_surface_ptr->surface));
    wlr_seat_pointer_notify_enter(
        wlmtk_env_wlr_seat(surface_ptr->super_element.env_ptr),
        wlr_scene_surface_ptr->surface,
        node_x, node_y);
    wlr_seat_pointer_notify_motion(
        wlmtk_env_wlr_seat(surface_ptr->super_element.env_ptr),
        time_msec,
        node_x, node_y);
    return true;
}

/* ------------------------------------------------------------------------- */
/**
 * Passes pointer button event further to the focused surface, if any.
 *
 * The actual passing is handled by `wlr_seat`. Here we just verify that the
 * currently-focused surface (or sub-surface) is part of this surface.
 *
 * @param element_ptr
 * @param button_event_ptr
 *
 * @return Whether the button event was consumed.
 */
bool _wlmtk_surface_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    wlmtk_surface_t *surface_ptr = BS_CONTAINER_OF(
        element_ptr, wlmtk_surface_t, super_element);

    // Complain if the surface isn't part of our responsibility.
    struct wlr_surface *focused_wlr_surface_ptr =
        wlmtk_env_wlr_seat(surface_ptr->super_element.env_ptr
            )->pointer_state.focused_surface;
    if (NULL == focused_wlr_surface_ptr) return false;
    // TODO(kaeser@gubbe.ch): Dragging the pointer from an activated window
    // over to a non-activated window will trigger the condition here on the
    // WLMTK_BUTTON_UP event. Needs a test and fixing.
    BS_ASSERT(surface_ptr->wlr_surface_ptr ==
              wlr_surface_get_root_surface(focused_wlr_surface_ptr));

    // We're only forwarding PRESSED & RELEASED events.
    if (WLMTK_BUTTON_DOWN == button_event_ptr->type ||
        WLMTK_BUTTON_UP == button_event_ptr->type) {
        wlr_seat_pointer_notify_button(
            wlmtk_env_wlr_seat(surface_ptr->super_element.env_ptr),
            button_event_ptr->time_msec,
            button_event_ptr->button,
            (button_event_ptr->type == WLMTK_BUTTON_DOWN) ?
            WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/**
 * Commits the given dimensions for the surface.
 *
 * @param surface_ptr
 * @param serial
 * @param width
 * @param height
 */
void _wlmtk_surface_commit_size(
    wlmtk_surface_t *surface_ptr,
    uint32_t serial,
    int width,
    int height)
{
    serial = serial;  // FIXME

    surface_ptr->committed_width = width;
    surface_ptr->committed_height = height;
}

/* ------------------------------------------------------------------------- */
/**
 * Handler for the `commit` signal of the `struct wlr_surface`.
 *
 * @param listener_ptr
 * @param data_ptr            Points to the const struct wlr_surface.
 */
void handle_surface_commit(
    struct wl_listener *listener_ptr,
    __UNUSED__ void *data_ptr)
{
    wlmtk_surface_t *surface_ptr = BS_CONTAINER_OF(
        listener_ptr, wlmtk_surface_t, surface_commit_listener);

    _wlmtk_surface_commit_size(
        surface_ptr,
        0,  // FIXME: Where do we get the serial from?
        surface_ptr->wlr_surface_ptr->current.width,
        surface_ptr->wlr_surface_ptr->current.height);
}

/* == Fake surface methods ================================================= */

static uint32_t _wlmtk_fake_surface_request_size(
    wlmtk_surface_t *surface_ptr,
    int width,
    int height);

/** Virtual method table for the fake surface. */
static const wlmtk_surface_vmt_t _wlmtk_fake_surface_vmt = {
    .request_size = _wlmtk_fake_surface_request_size,
};

/* ------------------------------------------------------------------------- */
wlmtk_fake_surface_t *wlmtk_fake_surface_create(void)
{
    wlmtk_fake_surface_t *fake_surface_ptr = logged_calloc(
        1, sizeof(wlmtk_fake_surface_t));
    if (NULL == fake_surface_ptr) return NULL;

    wlmtk_surface_init(&fake_surface_ptr->surface, NULL, NULL);
    wlmtk_surface_extend(&fake_surface_ptr->surface, &_wlmtk_fake_surface_vmt);
    return fake_surface_ptr;
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_surface_destroy(wlmtk_fake_surface_t *fake_surface_ptr)
{
    wlmtk_surface_fini(&fake_surface_ptr->surface);
    free(fake_surface_ptr);
}

/* ------------------------------------------------------------------------- */
void wlmtk_fake_surface_commit(wlmtk_fake_surface_t *fake_surface_ptr)
{
    _wlmtk_surface_commit_size(
        &fake_surface_ptr->surface,
        fake_surface_ptr->serial,
        fake_surface_ptr->requested_width,
        fake_surface_ptr->requested_height);
}

/* ------------------------------------------------------------------------- */
/** Fake implementation of @ref wlmtk_surface_vmt_t::request_size. */
uint32_t _wlmtk_fake_surface_request_size(
    wlmtk_surface_t *surface_ptr,
    int width,
    int height)
{
    wlmtk_fake_surface_t *fake_surface_ptr = BS_CONTAINER_OF(
        surface_ptr, wlmtk_fake_surface_t, surface);
    fake_surface_ptr->requested_width = width;
    fake_surface_ptr->requested_height = height;
    return fake_surface_ptr->serial;
}

/* == Unit tests =========================================================== */

static void test_init_fini(bs_test_t *test_ptr);
static void test_fake_commit(bs_test_t *test_ptr);

const bs_test_case_t wlmtk_surface_test_cases[] = {
    { 1, "init_fini", test_init_fini },
    { 1, "fake_commit", test_fake_commit },
    { 0, NULL, NULL }
};

/* ------------------------------------------------------------------------- */
/** Tests setup and teardown. */
void test_init_fini(bs_test_t *test_ptr)
{
    wlmtk_surface_t surface;

    BS_TEST_VERIFY_TRUE(test_ptr, wlmtk_surface_init(&surface, NULL, NULL));

    BS_TEST_VERIFY_EQ(
        test_ptr,
        &surface.super_element,
        wlmtk_surface_element(&surface));

    wlmtk_surface_fini(&surface);
}

/* ------------------------------------------------------------------------- */
/** Exercises the request_size / commit flow. */
void test_fake_commit(bs_test_t *test_ptr)
{
    wlmtk_fake_surface_t *fake_surface_ptr = wlmtk_fake_surface_create();
    int w, h;

    fake_surface_ptr->serial = 42;

    BS_TEST_VERIFY_EQ(
        test_ptr,
        42,
        wlmtk_surface_request_size(&fake_surface_ptr->surface, 200, 100));

    wlmtk_surface_get_size(&fake_surface_ptr->surface, &w, &h);
    BS_TEST_VERIFY_EQ(test_ptr, 0, w);
    BS_TEST_VERIFY_EQ(test_ptr, 0, h);

    wlmtk_fake_surface_commit(fake_surface_ptr);
    wlmtk_surface_get_size(&fake_surface_ptr->surface, &w, &h);
    BS_TEST_VERIFY_EQ(test_ptr, 200, w);
    BS_TEST_VERIFY_EQ(test_ptr, 100, h);

    wlmtk_fake_surface_destroy(fake_surface_ptr);
}

/* == End of surface.c ===================================================== */