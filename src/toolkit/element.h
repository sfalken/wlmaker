/* ========================================================================= */
/**
 * @file element.h
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
#ifndef __WLMTK_ELEMENT_H__
#define __WLMTK_ELEMENT_H__

#include <libbase/libbase.h>
#include <wayland-server.h>

/** Forward declaration: Element. */
typedef struct _wlmtk_element_t wlmtk_element_t;
/** Forward declaration: Element virtual method implementations. */
typedef struct _wlmtk_element_impl_t wlmtk_element_impl_t;

/** Forward declaration: Container. */
typedef struct _wlmtk_container_t wlmtk_container_t;
struct wlr_scene_tree;

#include "button.h"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

/** State of an element. */
struct _wlmtk_element_t {
    /** X position of the element, relative to the container. */
    int x;
    /** Y position of the element, relative to the container. */
    int y;

    /** The container this element belongs to, if any. */
    wlmtk_container_t         *parent_container_ptr;
    /** The node of elements. */
    bs_dllist_node_t          dlnode;

    /** Implementation of abstract virtual methods. */
    const wlmtk_element_impl_t *impl_ptr;

    /** Points to the wlroots scene graph API node, if attached. */
    struct wlr_scene_node     *wlr_scene_node_ptr;

    /** Whether the element is visible (drawn, when part of a scene graph). */
    bool                      visible;

    /** Listener for the `destroy` signal of `wlr_scene_node_ptr`. */
    struct wl_listener        wlr_scene_node_destroy_listener;

    /**
     * Horizontal pointer position from last @ref wlmtk_element_pointer_motion
     * call. NAN if there was no motion call yet, or if @ref
     * wlmtk_element_pointer_leave was called since.
     *
     * Does not imply that the element has pointer focus.
     */
    double                    last_pointer_x;
    /**
     * Vertical pointer position from last @ref wlmtk_element_pointer_motion
     * call. NAN if there was no motion call yet, or if @ref
     * wlmtk_element_pointer_leave was called since.
     *
     * Does not imply that the element has pointer focus.
     */
    double                    last_pointer_y;
    /** Time of last @ref wlmtk_element_pointer_motion call, 0 otherwise. */
    uint32_t                  last_pointer_time_msec;
};

/** Pointers to the implementation of Element's virtual methods. */
struct _wlmtk_element_impl_t {
    /** Destroys the implementation of the element. */
    void (*destroy)(wlmtk_element_t *element_ptr);

    /** Creates element's scene graph API node, child to wlr_scene_tree_ptr. */
    struct wlr_scene_node *(*create_scene_node)(
        wlmtk_element_t *element_ptr,
        struct wlr_scene_tree *wlr_scene_tree_ptr);

    /** Gets dimensions of the element, relative to the position. */
    void (*get_dimensions)(
        wlmtk_element_t *element_ptr,
        int *left_ptr,
        int *top_ptr,
        int *right_ptr,
        int *bottom_ptr);

    /** Gets element area to accept pointer activity, relative to position. */
    void (*get_pointer_area)(
        wlmtk_element_t *element_ptr,
        int *left_ptr,
        int *top_ptr,
        int *right_ptr,
        int *bottom_ptr);

    /**
     * Indicates pointer motion into or within the element area to (x,y).
     *
     * @param element_ptr
     * @param x
     * @param y
     * @param time_msec
     *
     * @return Whether the motion is considered within the element's pointer
     *     area. If it returns true, the caller should consider this element
     *     as having pointer focus.
     */
    bool (*pointer_motion)(wlmtk_element_t *element_ptr,
                                       double x, double y,
                                       uint32_t time_msec);
    /** Indicates pointer button event. */
    bool (*pointer_button)(wlmtk_element_t *element_ptr,
                           const wlmtk_button_event_t *button_event_ptr);

    /** Indicates the pointer has left the element's area. */
    void (*pointer_leave)(wlmtk_element_t *element_ptr);
};

/**
 * Initializes the element.
 *
 * @param element_ptr
 * @param element_impl_ptr
 *
 * @return true on success.
 */
bool wlmtk_element_init(
    wlmtk_element_t *element_ptr,
    const wlmtk_element_impl_t *element_impl_ptr);

/**
 * Cleans up the element.
 *
 * @param element_ptr
 */
void wlmtk_element_fini(
    wlmtk_element_t *element_ptr);

/** Gets the dlnode from the element. */
bs_dllist_node_t *wlmtk_dlnode_from_element(
    wlmtk_element_t *element_ptr);
/** Gets the element from the dlnode. */
wlmtk_element_t *wlmtk_element_from_dlnode(
    bs_dllist_node_t *dlnode_ptr);

/**
 * Sets the parent container for the element.
 *
 * Will call @ref wlmtk_element_attach_to_scene_graph to align the scene graph
 * with the new (or deleted) parent.
 *
 * Private: Should only be called by wlmtk_container_add_element, respectively
 * wlmtk_container_remove_element ("friends").
 *
 * @param element_ptr
 * @param parent_container_ptr Pointer to the parent container, or NULL if
 *     the parent should be cleared.
 */
void wlmtk_element_set_parent_container(
    wlmtk_element_t *element_ptr,
    wlmtk_container_t *parent_container_ptr);

/**
 * Attaches or detaches the element to the parent's wlroots scene tree.
 *
 * If the element has a parent, and that parent is itself attached to the
 * wlroots scene tree, this will either re-parent an already existing node,
 * or invoke wlmtk_element_impl_t::create_scene_node to create and attach a
 * new node to the paren'ts tree.
 * Otherwise, it will clear any existing node.
 *
 * The function is idempotent.
 *
 * Private: Should only called by wlmtk_container_t methods, when there are
 * changes to wlmtk_container_t::wlr_scene_tree.
 *
 * @param element_ptr
 */
void wlmtk_element_attach_to_scene_graph(
    wlmtk_element_t *element_ptr);

/**
 * Sets the element's visibility.
 *
 * @param element_ptr
 * @param visible
 */
void wlmtk_element_set_visible(wlmtk_element_t *element_ptr, bool visible);

/**
 * Returns the position of the element.
 *
 * @param element_ptr
 * @param x_ptr               Optional, may be NULL.
 * @param y_ptr               Optional, may be NULL.
 */
void wlmtk_element_get_position(
    wlmtk_element_t *element_ptr,
    int *x_ptr,
    int *y_ptr);

/**
 * Sets the position of the element.
 *
 * @param element_ptr
 * @param x
 * @param y
 */
void wlmtk_element_set_position(
    wlmtk_element_t *element_ptr,
    int x,
    int y);

/**
 * Gets the dimensions of the element in pixels, relative to the position.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position. May be NULL.
 * @param top_ptr             Topmost position. May be NULL.
 * @param right_ptr           Rightmost position. Ma be NULL.
 * @param bottom_ptr          Bottommost position. May be NULL.
 */
void wlmtk_element_get_dimensions(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);

/**
 * Gets the area that the element on which the element accepts pointer events.
 *
 * The area extents are relative to the element's position. By default, this
 * overlaps with the element dimensions. Some elements (eg. a surface with
 * further-extending sub-surfaces) may differ.
 *
 * @param element_ptr
 * @param left_ptr            Leftmost position of pointer area. May be NULL.
 * @param top_ptr             Topmost position of pointer area. May be NULL.
 * @param right_ptr           Rightmost position of pointer area. May be NULL.
 * @param bottom_ptr          Bottommost position of pointer area. May be NULL.
 */
void wlmtk_element_get_pointer_area(
    wlmtk_element_t *element_ptr,
    int *left_ptr,
    int *top_ptr,
    int *right_ptr,
    int *bottom_ptr);

/**
 * Virtual method: Calls 'pointer_motion' for the element's implementation.
 *
 * Also updates wlmtk_element_t::last_pointer_x,
 * wlmtk_element_t::last_pointer_y and wlmtk_element_t::last_pointer_time_msec.
 *
 * @param element_ptr
 * @param x
 * @param y
 * @param time_msec
 *
 * @return Whether the coordinates are within this element's area that accepts
 *     pointer events. May be a subset of @ref wlmtk_element_get_pointer_area.
 *
 */
bool wlmtk_element_pointer_motion(
    wlmtk_element_t *element_ptr,
    double x,
    double y,
    uint32_t time_msec);

/**
 * Virtual method: Calls 'pointer_leave' for the element's implementation.
 */
void wlmtk_element_pointer_leave(
    wlmtk_element_t *element_ptr);

/** Virtual method: calls 'button' for the element's implementation. */
static inline bool wlmtk_element_pointer_button(
    wlmtk_element_t *element_ptr,
    const wlmtk_button_event_t *button_event_ptr)
{
    if (NULL == element_ptr->impl_ptr->pointer_button) return false;
    return element_ptr->impl_ptr->pointer_button(
        element_ptr, button_event_ptr);
}

/**
 * Virtual method: Calls the dtor of the element's implementation.
 *
 * The implementation is required to call @ref wlmtk_element_fini().
 *
 * @param element_ptr
 */
static inline void wlmtk_element_destroy(
    wlmtk_element_t *element_ptr) {
    element_ptr->impl_ptr->destroy(element_ptr);
}

/** Unit tests for the element. */
extern const bs_test_case_t wlmtk_element_test_cases[];

/** Fake element, useful for unit test. */
typedef struct {
    /** State of the element. */
    wlmtk_element_t           element;
    /** Width of the element, in pixels. */
    int width;
    /** Height of the element, in pixels. */
    int height;

    /** Indicates that Element::pointer_motion() was called. */
    bool                      pointer_motion_called;

    /** Indicates that Element::pointer_leave() was called. */
    bool                      pointer_leave_called;

    /** Indicates that Element::pointer_button() was called. */
    bool                      pointer_button_called;
    /** Last button event reveiced. */
    wlmtk_button_event_t      pointer_button_event;
} wlmtk_fake_element_t;

/** Ctor for the fake element. */
wlmtk_fake_element_t *wlmtk_fake_element_create(void);

/** Implementation table of a "fake" element for tests. */
extern const wlmtk_element_impl_t wlmtk_fake_element_impl;

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* __WLMTK_ELEMENT_H__ */
/* == End of element.h ===================================================== */
