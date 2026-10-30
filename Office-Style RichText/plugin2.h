#pragma once

#include <stdint.h>
struct INPUT_PLUGIN_TABLE;
struct OUTPUT_PLUGIN_TABLE;
struct FILTER_PLUGIN_TABLE;
struct SCRIPT_MODULE_TABLE;
struct EDIT_HANDLE;
struct PROJECT_FILE;
struct PIXEL_RGBA;
struct IDWriteFontCollection;
struct IDWriteFont;

struct COMMON_PLUGIN_TABLE {
	LPCWSTR name;
	LPCWSTR information;
};

typedef void* OBJECT_HANDLE;

typedef void* EFFECT_HANDLE;

struct OBJECT_LAYER_FRAME {
	int layer;
	int start;
	int end;
};

struct MEDIA_INFO {
	int video_track_num;
	int audio_track_num;
	double total_time;
	int width, height;
};

struct MODULE_INFO {
	int type;
	static constexpr int TYPE_SCRIPT_FILTER	= 1;
	static constexpr int TYPE_SCRIPT_OBJECT	= 2;
	static constexpr int TYPE_SCRIPT_CAMERA	= 3;
	static constexpr int TYPE_SCRIPT_TRACK	= 4;
	static constexpr int TYPE_SCRIPT_MODULE	= 5;
	static constexpr int TYPE_PLUGIN_INPUT	= 6;
	static constexpr int TYPE_PLUGIN_OUTPUT	= 7;
	static constexpr int TYPE_PLUGIN_FILTER	= 8;
	static constexpr int TYPE_PLUGIN_COMMON	= 9;
	LPCWSTR name;
	LPCWSTR information;
};

struct TRACK_INFO {
	LPCWSTR mode;
	double* param;
	int param_num;
	bool accelerate;
	bool decelerate;
	bool twopoint;
	bool timecontrol;
	int group_num;
	int group_index;
	LPCWSTR group_name;
};

struct PALETTE_INFO {
	static constexpr int PALETTE_NUM = 64;
	struct {
		unsigned char r, g, b, a;
	} color[PALETTE_NUM];
};

struct BPM_INFO {
	float tempo;
	int	beat;
	double start;
	float offset;
};

enum class EVENT_TYPE : int {
	UPDATE_OBJECT = 1,
	CHANGE_EDIT_FRAME = 2,
	CHANGE_EDIT_SCENE = 3,
	CHANGE_FOCUS_OBJECT = 4,
	CHANGE_EDIT_STATE = 5,
};

enum class OBJECT_FLAG_TYPE : int {
	ENABLE_GROUP = 1,
	ENABLE_CAMERA = 2,
	CLIPPING_OBJECT = 3,
	CLIPPING_UPPER_OBJECT = 4,
};

struct EDIT_INFO {
	int width, height;
	int rate, scale;
	int sample_rate;
	int frame;
	int layer;
	int frame_max;
	int layer_max;
	int display_frame_start;
	int display_layer_start;
	int display_frame_num;
	int display_layer_num;
	int select_range_start;
	int select_range_end;
	float grid_bpm_tempo;
	int grid_bpm_beat;
	float grid_bpm_offset;
	int scene_id;
	struct COLOR {
		unsigned char r, g, b, a;
	};
	COLOR background;
};

struct EDIT_SECTION {

	EDIT_INFO* info;

	OBJECT_HANDLE (*create_object_from_alias)(LPCSTR alias, int layer, int frame, int length);

	OBJECT_HANDLE (*find_object)(int layer, int frame);

	int (*count_object_effect)(OBJECT_HANDLE object, LPCWSTR effect);

	OBJECT_LAYER_FRAME (*get_object_layer_frame)(OBJECT_HANDLE object);

	LPCSTR (*get_object_alias)(OBJECT_HANDLE object);

	LPCSTR (*get_object_item_value)(OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR item);

	bool (*set_object_item_value)(OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR item, LPCSTR value);

	bool (*move_object)(OBJECT_HANDLE object, int layer, int frame);

	void (*delete_object)(OBJECT_HANDLE object);

	OBJECT_HANDLE (*get_focus_object)();

	void (*set_focus_object)(OBJECT_HANDLE object);

	PROJECT_FILE* (*get_project_file)(EDIT_HANDLE* edit);

	OBJECT_HANDLE (*get_selected_object)(int index);

	int (*get_selected_object_num)();

	bool (*get_mouse_layer_frame)(int* layer, int* frame);

	bool (*pos_to_layer_frame)(int x, int y, int* layer, int* frame);

	bool (*is_support_media_file)(LPCWSTR file, bool strict);

	bool (*get_media_info)(LPCWSTR file, MEDIA_INFO* info, int info_size);

	OBJECT_HANDLE (*create_object_from_media_file)(LPCWSTR file, int layer, int frame, int length);

	OBJECT_HANDLE (*create_object)(LPCWSTR effect, int layer, int frame, int length);

	void (*set_cursor_layer_frame)(int layer, int frame);

	void (*set_display_layer_frame)(int layer, int frame);

	void (*set_select_range)(int start, int end);

	void (*set_grid_bpm)(float tempo, int beat, float offset);

	LPCWSTR (*get_object_name)(OBJECT_HANDLE object);

	void (*set_object_name)(OBJECT_HANDLE object, LPCWSTR name);

	LPCWSTR (*get_layer_name)(int layer);

	void (*set_layer_name)(int layer, LPCWSTR name);

	LPCWSTR (*get_scene_name)();

	void (*set_scene_name)(LPCWSTR name);

	void (*set_scene_size)(int width, int height);

	void (*set_scene_frame_rate)(int rate, int scale);

	void (*set_scene_sample_rate)(int sample_rate);

	bool (*get_layer_enable)(int layer);

	void (*set_layer_enable)(int layer, bool enable);

	bool (*get_layer_lock)(int layer);

	void (*set_layer_lock)(int layer, bool lock);

	int (*get_object_section_num)(OBJECT_HANDLE object);

	int (*get_focus_object_section)();

	int (*get_object_section_frame)(OBJECT_HANDLE object, int section);

	bool (*get_object_track_value)(OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR item, double frame, double* value);

	bool (*get_object_check_value)(OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR item, int frame, bool* value);

	bool (*get_object_track_info)(OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR item, TRACK_INFO* info, int info_size);

	LPCWSTR (*get_palette_name)();

	bool (*get_palette_info)(LPCWSTR name, PALETTE_INFO* info, int info_size);

	IDWriteFont* (*get_font)(LPCWSTR font);

	int (*get_object_track_group_names)(OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR group_name, LPCWSTR* item_names, int item_num);

	int (*deprecated_get_grid_bpm_list)(BPM_INFO* bpm_list, int bpm_num);

	void (*deprecated_set_grid_bpm_list)(BPM_INFO* bpm_list, int bpm_num);

	EFFECT_HANDLE (*find_effect)(OBJECT_HANDLE object, LPCWSTR effect);

	int (*get_effect_list)(OBJECT_HANDLE object, EFFECT_HANDLE* effect_list, int effect_num);

	LPCWSTR (*get_effect_name)(EFFECT_HANDLE effect);

	bool (*get_effect_enable)(EFFECT_HANDLE effect);

	void (*set_effect_enable)(EFFECT_HANDLE effect, bool enable);

	bool (*get_effect_lock)(EFFECT_HANDLE effect);

	void (*set_effect_lock)(EFFECT_HANDLE effect, bool lock);

	LPCSTR (*get_effect_item_value)(EFFECT_HANDLE effect, LPCWSTR item);

	bool (*set_effect_item_value)(EFFECT_HANDLE effect, LPCWSTR item, LPCSTR value);

	bool (*get_effect_track_value)(EFFECT_HANDLE effect, LPCWSTR item, double frame, double* value);

	bool (*get_effect_check_value)(EFFECT_HANDLE effect, LPCWSTR item, int frame, bool* value);

	bool (*get_effect_track_info)(EFFECT_HANDLE effect, LPCWSTR item, TRACK_INFO* info, int info_size);

	int (*get_grid_bpm_list)(BPM_INFO* bpm_list, int bpm_num, int bpm_size);

	void (*set_grid_bpm_list)(BPM_INFO* bpm_list, int bpm_num, int bpm_size);

	EFFECT_HANDLE (*create_effect)(OBJECT_HANDLE object, LPCWSTR effect);

	bool (*delete_effect)(OBJECT_HANDLE object, EFFECT_HANDLE effect);

	bool (*create_object_section)(OBJECT_HANDLE object, int frame);

	bool (*delete_object_section)(OBJECT_HANDLE object, int section);

	bool (*move_object_section)(OBJECT_HANDLE object, int section, int frame);

	int (*move_effect)(OBJECT_HANDLE object, EFFECT_HANDLE effect, int index);

	int (*get_effect_data_value)(EFFECT_HANDLE effect, LPCWSTR item, void *data, int size);

	bool (*set_effect_data_value)(EFFECT_HANDLE effect, LPCWSTR item, void* data, int size);

	void (*set_edited_state)();

	int (*get_mark_frame_list)(int* frame_list, int frame_num);

	LPCWSTR (*get_mark_frame_memo)(int frame);

	void (*set_mark_frame)(int frame, LPCWSTR memo);

	void (*clear_mark_frame)(int frame);

	bool (*move_mark_frame)(int frame, int frame_to);

	bool (*set_palette_info)(LPCWSTR name, PALETTE_INFO* info, int info_size);

	bool (*get_object_flag)(OBJECT_HANDLE object, OBJECT_FLAG_TYPE type);

	void (*set_object_flag)(OBJECT_HANDLE object, OBJECT_FLAG_TYPE type, bool flag);

	int64_t (*get_object_id)(OBJECT_HANDLE object);

	int64_t (*get_effect_id)(EFFECT_HANDLE effect);

	PIXEL_RGBA (*get_layer_color)(int layer);

	void (*set_layer_color)(int layer, PIXEL_RGBA color);

};

struct EDIT_HANDLE {

	bool (*call_edit_section)(void (*func_proc_edit)(EDIT_SECTION* edit));

	bool (*call_edit_section_param)(void* param, void (*func_proc_edit)(void* param, EDIT_SECTION* edit));

	void (*get_edit_info)(EDIT_INFO* info, int info_size);

	void (*restart_host_app)();

	void (*enum_effect_name)(void* param, void (*func_proc_enum_effect)(void* param, LPCWSTR name, int type, int flag));

	static constexpr int EFFECT_TYPE_FILTER		= 1;
	static constexpr int EFFECT_TYPE_INPUT		= 2;
	static constexpr int EFFECT_TYPE_TRANSITION = 3;
	static constexpr int EFFECT_TYPE_CONTROL	= 4;
	static constexpr int EFFECT_TYPE_OUTPUT		= 5;

	static constexpr int EFFECT_FLAG_VIDEO		= 1;
	static constexpr int EFFECT_FLAG_AUDIO		= 2;
	static constexpr int EFFECT_FLAG_FILTER		= 4;
	static constexpr int EFFECT_FLAG_CAMERA		= 8;

	void (*enum_module_info)(void* param, void (*func_proc_enum_module)(void* param, MODULE_INFO* info));

	HWND (*get_host_app_window)();

	int (*get_edit_state)();
	static constexpr int EDIT_STATE_EDIT = 0;
	static constexpr int EDIT_STATE_PLAY = 1;
	static constexpr int EDIT_STATE_SAVE = 2;

	bool (*call_read_section)(void (*func_proc_read_section)(EDIT_SECTION* edit));

	bool (*call_read_section_param)(void* param, void (*func_proc_read_section)(void* param, EDIT_SECTION* edit));

	bool (*enum_effect_item)(LPCWSTR effect, void* param, void (*func_proc_enum_effect_item)(void* param, LPCWSTR name, int type));

	static constexpr int EFFECT_ITEM_TYPE_INTEGER	= 1;
	static constexpr int EFFECT_ITEM_TYPE_NUMBER	= 2;
	static constexpr int EFFECT_ITEM_TYPE_CHECK		= 3;
	static constexpr int EFFECT_ITEM_TYPE_TEXT		= 4;
	static constexpr int EFFECT_ITEM_TYPE_STRING	= 5;
	static constexpr int EFFECT_ITEM_TYPE_FILE		= 6;
	static constexpr int EFFECT_ITEM_TYPE_COLOR		= 7;
	static constexpr int EFFECT_ITEM_TYPE_SELECT	= 8;
	static constexpr int EFFECT_ITEM_TYPE_SCENE		= 9;
	static constexpr int EFFECT_ITEM_TYPE_RANGE		= 10;
	static constexpr int EFFECT_ITEM_TYPE_COMBO		= 11;
	static constexpr int EFFECT_ITEM_TYPE_MASK		= 12;
	static constexpr int EFFECT_ITEM_TYPE_FONT		= 13;
	static constexpr int EFFECT_ITEM_TYPE_FIGURE	= 14;
	static constexpr int EFFECT_ITEM_TYPE_DATA		= 15;
	static constexpr int EFFECT_ITEM_TYPE_FOLDER	= 16;
	static constexpr int EFFECT_ITEM_TYPE_NUMBER_GROUP	= 17;
	static constexpr int EFFECT_ITEM_TYPE_GROUP			= 18;
	static constexpr int EFFECT_ITEM_TYPE_SEPARATOR		= 19;

	bool (*rendering_scene_video)(int frame, void* param, void (*func_proc_rendering_video)(void* param, int frame, const void* buffer, int width, int height, int pitch));

	bool (*rendering_scene_audio)(int frame, void* param, void (*func_proc_rendering_audio)(void* param, int frame, const float* buffer0, const float* buffer1, int sample_num));

	void (*wait_rendering_task)();

	void (*enum_font_name)(void* param, void (*func_proc_enum_font)(void* param, LPCWSTR name));

	void (*enum_palette_name)(void* param, void (*func_proc_enum_palette)(void* param, LPCWSTR name));

	bool (*rendering_object_video)(OBJECT_HANDLE object, int frame, bool apply_effect, void* param, void (*func_proc_rendering_video)(void* param, int frame, const void* buffer, int width, int height, int pitch));

	bool (*rendering_object_audio)(OBJECT_HANDLE object, int frame, bool apply_effect, void* param, void (*func_proc_rendering_audio)(void* param, int frame, const float* buffer0, const float* buffer1, int sample_num));

	int (*get_effect_item_group_names)(LPCWSTR effect, LPCWSTR item, LPCWSTR* item_names, int item_num, int* item_index);

	void (*enum_scene_name)(void* param, void (*func_proc_enum_scene)(void* param, LPCWSTR name, int scene_id));

	bool (*select_scene)(int scene_id);

	bool (*create_scene)(LPCWSTR name, LPCWSTR label, int width, int height, int rate, int scale, int sample_rate, EDIT_INFO::COLOR background);

	bool (*create_project)(int width, int height, int rate, int scale, int sample_rate, EDIT_INFO::COLOR background, bool show_confirm);

	bool (*open_project_file)(LPCWSTR file, bool show_confirm);

	bool (*save_project_file)(LPCWSTR file);

	bool (*output_file)(LPCWSTR file, LPCWSTR output_plugin, void* param, void (*func_project_config)(void* param, PROJECT_FILE* project));

	float (*get_high_dpi_ratio)();

};

struct PROJECT_FILE {

	LPCSTR (*get_param_string)(LPCSTR key);

	void (*set_param_string)(LPCSTR key, LPCSTR value);

	bool (*get_param_binary)(LPCSTR key, void* data, int size);

	void (*set_param_binary)(LPCSTR key, void* data, int size);

	void (*clear_params)();

	LPCWSTR (*get_project_file_path)();

};

struct HOST_APP_TABLE {

	void (*set_plugin_information)(LPCWSTR information);

	void (*register_input_plugin)(INPUT_PLUGIN_TABLE* input_plugin_table);

	void (*register_output_plugin)(OUTPUT_PLUGIN_TABLE* output_plugin_table);

	void (*register_filter_plugin)(FILTER_PLUGIN_TABLE* filter_plugin_table);

	void (*register_script_module)(SCRIPT_MODULE_TABLE* script_module_table);

	void (*register_import_menu)(LPCWSTR name, void (*func_proc_import)(EDIT_SECTION* edit));

	void (*register_export_menu)(LPCWSTR name, void (*func_proc_export)(EDIT_SECTION* edit));

	void (*register_window_client)(LPCWSTR name, HWND hwnd);

	EDIT_HANDLE* (*create_edit_handle)();

	void (*register_project_load_handler)(void (*func_project_load)(PROJECT_FILE* project));

	void (*register_project_save_handler)(void (*func_project_save)(PROJECT_FILE* project));

	void (*register_layer_menu)(LPCWSTR name, void (*func_proc_layer_menu)(EDIT_SECTION* edit));

	void (*register_object_menu)(LPCWSTR name, void (*func_proc_object_menu)(EDIT_SECTION* edit));

	void (*register_config_menu)(LPCWSTR name, void (*func_config)(HWND hwnd, HINSTANCE dll_hinst));

	void (*register_edit_menu)(LPCWSTR name, void (*func_proc_edit_menu)(EDIT_SECTION* edit));

	void (*register_clear_cache_handler)(void (*func_proc_clear_cache)(EDIT_SECTION* edit));

	void (*register_change_scene_handler)(void (*func_proc_change_scene)(EDIT_SECTION* edit));

	void (*register_import_menu_param)(LPCWSTR name, void* param, void (*func_proc_import)(void* param));

	void (*register_export_menu_param)(LPCWSTR name, void* param, void (*func_proc_export)(void* param));

	void (*register_layer_menu_param)(LPCWSTR name, void* param, void (*func_proc_layer_menu)(void* param));

	void (*register_object_menu_param)(LPCWSTR name, void* param, void (*func_proc_object_menu)(void* param));

	void (*register_edit_menu_param)(LPCWSTR name, void* param, void (*func_proc_edit_menu)(void* param));

	void (*register_file_drop_handler)(LPCWSTR name, LPCWSTR filefilter, void (*func_proc_file_drop)(EDIT_SECTION* edit, LPCWSTR file));

	void (*register_file_drop_param_handler)(LPCWSTR name, LPCWSTR filefilter, void* param, void (*func_proc_file_drop)(void* param, LPCWSTR file));

	void (*register_object_item_menu)(LPCWSTR name, bool allow_effect_only, void (*func_proc_item_menu)(EDIT_SECTION* edit, OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR item));

	void (*register_object_item_menu_param)(LPCWSTR name, bool allow_effect_only, void* param, void (*func_proc_item_menu)(void* param, OBJECT_HANDLE object, LPCWSTR effect, LPCWSTR item));

	void (*register_script_module_name)(SCRIPT_MODULE_TABLE* script_module_table, LPCWSTR module_name);

	void (*register_font_collection)(IDWriteFontCollection* collection);

	void (*register_event_listener)(EVENT_TYPE type, void* param, void (*func_proc_event)(void* param));

};
