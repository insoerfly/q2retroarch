/*
 * launcher_main.c - custom AOS application (.app) skeleton for MP5 X7 / ATJ2273S.
 *
 * Modeled on the USDK app template (see case/apps/desktop/stopwatch).
 * Build: see Makefile + build.sh (MIPS SDE / mipsel-linux-gnu via SDK).
 *
 * NOT part of the atj2273sdk repository (separate project).
 */
#include "launcher.h"

static const clk_domain_t s_clk_domain =
{ CLK_LOAD_MIN, CLK_LOAD_HIGH_0 };

typedef struct app_s
{
    void *applib_lib;
    void *commonui_lib;
    void *fusion_lib;
    void *gui_lib;
    void *sty_lib;
    void *config_lib;
} app_t;

static bool _lib_init(app_t *data);
static bool _lib_deinit(app_t *data);

static bool _lib_init(app_t *data)
{
    data->applib_lib  = dlopen("applib.so", RTLD_LAZY);
    if (data->applib_lib == NULL)  { print_err("open applib.so err\n");  goto err; }
    data->commonui_lib = dlopen("commonui.so", RTLD_LAZY);
    if (data->commonui_lib == NULL) { print_err("open commonui.so err\n"); goto err; }
    data->fusion_lib  = dlopen("fusion.so", RTLD_LAZY);
    if (data->fusion_lib == NULL)  { print_err("open fusion.so err\n");  goto err; }
    data->gui_lib     = dlopen("gui.so", RTLD_LAZY);
    if (data->gui_lib == NULL)     { print_err("open gui.so err\n");     goto err; }
    data->sty_lib     = dlopen("style.so", RTLD_LAZY);
    if (data->sty_lib == NULL)     { print_err("open style.so err\n");   goto err; }
    data->config_lib  = dlopen("apconfig.so", RTLD_LAZY);
    if (data->config_lib == NULL)  { print_err("open apconfig.so err\n"); goto err; }
    return true;

err:
    _lib_deinit(data);
    return false;
}

static bool _lib_deinit(app_t *data)
{
    if (data->config_lib  != NULL) { dlclose(data->config_lib);  data->config_lib  = NULL; }
    if (data->sty_lib     != NULL) { dlclose(data->sty_lib);     data->sty_lib     = NULL; }
    if (data->gui_lib     != NULL) { dlclose(data->gui_lib);     data->gui_lib     = NULL; }
    if (data->fusion_lib  != NULL) { dlclose(data->fusion_lib);  data->fusion_lib  = NULL; }
    if (data->commonui_lib != NULL){ dlclose(data->commonui_lib);data->commonui_lib= NULL; }
    if (data->applib_lib  != NULL) { dlclose(data->applib_lib);  data->applib_lib  = NULL; }
    return true;
}

int main(int argc, char *argv[])
{
    app_t app_data;

    if (_lib_init(&app_data) == false)
    {
        print_err("app init error!\n");
        return -1;
    }

    applib_init(argc, argv, APP_TYPE_GUI);
    sys_pm_set_clk_domain(&s_clk_domain);

    launcher_scene(FUSION_FORWARD);

    sys_pm_clear_clk_domain(&s_clk_domain);
    applib_quit();
    _lib_deinit(&app_data);
    return 0;
}
