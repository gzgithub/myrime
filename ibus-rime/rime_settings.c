#include <ibus.h>
#include "rime_settings.h"

static struct ColorSchemeDefinition preset_color_schemes[] = {
  { "aqua", 0xffffff, 0x0a3dfa },
  { "azure", 0xffffff, 0x0a3dea },
  { "ink", 0xffffff, 0x000000 },
  { "luna", 0x000000, 0xffff7f },
  { NULL, 0, 0 }
};

struct IBusRimeSettings g_ibus_rime_settings = {
  FALSE,
  IBUS_ORIENTATION_SYSTEM,
  &preset_color_schemes[0],
};

static void
ibus_rime_select_color_scheme(const char* color_scheme_id)
{
  struct ColorSchemeDefinition* c;
  for (c = preset_color_schemes; c->color_scheme_id; ++c) {
    if (!strcmp(c->color_scheme_id, color_scheme_id)) {
      g_ibus_rime_settings.color_scheme = c;
      g_debug("selected color scheme: %s", color_scheme_id);
      return;
    }
  }
  g_ibus_rime_settings.color_scheme = &preset_color_schemes[0];
}

void
ibus_rime_load_settings(IBusConfig* config)
{
  //g_debug("ibus_rime_load_settings");
  GValue value = {0};
  gboolean bl;

  bl = ibus_config_get_value(config, "engine/Rime", "embed_preedit_text", &value);
  if (!bl) {
    bl = ibus_config_get_value(config, "general", "embed_preedit_text", &value);
  }

  if (G_VALUE_TYPE(&value) == G_TYPE_BOOLEAN) {
    g_ibus_rime_settings.embed_preedit_text = g_value_get_boolean(&value);
  }

  bl = ibus_config_get_value(config, "engine/Rime", "lookup_table_orientation", &value);
  if (!bl) {
    bl = ibus_config_get_value(config, "general", "lookup_table_orientation", &value);
  }
  if (G_VALUE_TYPE(&value) == G_TYPE_INT) {
    g_ibus_rime_settings.lookup_table_orientation = g_value_get_int(&value);
  }

  bl = ibus_config_get_value(config, "engine/Rime", "color_scheme", &value);
  if (G_VALUE_TYPE(&value) == G_TYPE_STRING) {
    ibus_rime_select_color_scheme(g_value_get_string(&value));
  }
}

void
ibus_rime_config_value_changed_cb(IBusConfig* config,
                                  const gchar* section,
                                  const gchar* name,
                                  GVariant* value,
                                  gpointer unused)
{
  //g_debug("ibus_rime_config_value_changed_cb [%s/%s]", section, name);
  if (!strcmp("general", section)) {
    if (!strcmp("embed_preedit_text", name) &&
        value && g_variant_classify(value) == G_VARIANT_CLASS_BOOLEAN) {
      g_ibus_rime_settings.embed_preedit_text = g_variant_get_boolean(value);
      return;
    }
    if (!strcmp("lookup_table_orientation", name) &&
        value && g_variant_classify(value) == G_VARIANT_CLASS_INT32) {
      g_ibus_rime_settings.lookup_table_orientation = g_variant_get_int32(value);
      return;
    }
  }
  else if (!strcmp("engine/Rime", section)) {
    if (!strcmp("embed_preedit_text", name) &&
        value && g_variant_classify(value) == G_VARIANT_CLASS_BOOLEAN) {
      GValue overridden = {0};
      gboolean bl = ibus_config_get_value(config, "engine/Rime", "embed_preedit_text", &overridden);
      if (!bl) {
        g_ibus_rime_settings.embed_preedit_text = g_variant_get_boolean(value);
      }
      return;
    }
    if (!strcmp("lookup_table_orientation", name) &&
        value && g_variant_classify(value) == G_VARIANT_CLASS_INT32) {
      GValue overridden = {0};
      gboolean bl = ibus_config_get_value(config, "engine/Rime", "lookup_table_orientation", &overridden);
      if (!bl) {
        g_ibus_rime_settings.lookup_table_orientation = g_variant_get_int32(value);
      }
      return;
    }
    if (!strcmp("color_scheme", name) &&
        value && g_variant_classify(value) == G_VARIANT_CLASS_STRING) {
      ibus_rime_select_color_scheme(g_variant_get_string(value, NULL));
      return;
    }
  }
}
