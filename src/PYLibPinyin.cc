/* vim:set et ts=4 sts=4:
 *
 * ibus-pinyin - The Chinese PinYin engine for IBus
 *
 * Copyright (c) 2011 Peng Wu <alexepico@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "PYLibPinyin.h"
#include "PYTypes.h"
#include "PYConfig.h"

#define LIBPINYIN_SAVE_TIMEOUT   (5 * 60)

using namespace PY;

std::unique_ptr<LibPinyinBackEnd> LibPinyinBackEnd::m_instance;

static LibPinyinBackEnd libpinyin_backend;

LibPinyinBackEnd::LibPinyinBackEnd () {
    m_timeout_id = 0;
    m_timer = g_timer_new();
    m_pinyin_context = NULL;
    m_chewing_context = NULL;
}

LibPinyinBackEnd::~LibPinyinBackEnd () {
    g_timer_destroy (m_timer);
    if (m_timeout_id != 0) {
        saveUserDB ();
        g_source_remove (m_timeout_id);
    }

    if (m_pinyin_context)
        pinyin_fini(m_pinyin_context);
    m_pinyin_context = NULL;
    if (m_chewing_context)
        pinyin_fini(m_chewing_context);
    m_chewing_context = NULL;
}

pinyin_instance_t *
LibPinyinBackEnd::allocPinyinInstance ()
{
    if (NULL == m_pinyin_context) {
        gchar * userdir = g_build_filename (g_get_home_dir(), ".cache",
                                            "ibus", "libpinyin", NULL);
        int retval = g_mkdir_with_parents (userdir, 0700);
        if (retval) {
            g_free(userdir); userdir = NULL;
        }
        m_pinyin_context = pinyin_init ("/usr/share/libpinyin/data", userdir);
        setPinyinOptions (&PinyinConfig::instance ());
        g_free(userdir);
    }
    return pinyin_alloc_instance (m_pinyin_context);
}

void
LibPinyinBackEnd::freePinyinInstance (pinyin_instance_t *instance)
{
    pinyin_free_instance (instance);
}

pinyin_instance_t *
LibPinyinBackEnd::allocChewingInstance ()
{
    if (NULL == m_chewing_context) {
        gchar * userdir = g_build_filename (g_get_home_dir(), ".cache",
                                            "ibus", "libbopomofo", NULL);
        int retval = g_mkdir_with_parents (userdir, 0700);
        if (retval) {
            g_free(userdir); userdir = NULL;
        }
        m_chewing_context = pinyin_init ("/usr/share/libpinyin/data", NULL);
        setChewingOptions (&BopomofoConfig::instance ());
        g_free(userdir);
    }
    return pinyin_alloc_instance (m_chewing_context);
}

void
LibPinyinBackEnd::freeChewingInstance (pinyin_instance_t *instance)
{
    pinyin_free_instance (instance);
}

void
LibPinyinBackEnd::init (void) {
    g_assert (NULL == m_instance.get ());
    LibPinyinBackEnd * backend = new LibPinyinBackEnd;
    m_instance.reset (backend);
}

void
LibPinyinBackEnd::finalize (void) {
    m_instance.reset ();
}

/* Here are the fuzzy pinyin options conversion table. */
static const struct {
    guint ibus_pinyin_option;
    PinyinAmbiguity libpinyin_option;
} fuzzy_options [] = {
    /* fuzzy pinyin */
    { PINYIN_FUZZY_C_CH,        PINYIN_AmbCiChi        },
    { PINYIN_FUZZY_CH_C,        PINYIN_AmbChiCi        },
    { PINYIN_FUZZY_Z_ZH,        PINYIN_AmbZiZhi        },
    { PINYIN_FUZZY_ZH_Z,        PINYIN_AmbZhiZi        },
    { PINYIN_FUZZY_S_SH,        PINYIN_AmbSiShi        },
    { PINYIN_FUZZY_SH_S,        PINYIN_AmbShiSi        },
    { PINYIN_FUZZY_L_N,         PINYIN_AmbLeNe         },
    { PINYIN_FUZZY_N_L,         PINYIN_AmbNeLe         },
    { PINYIN_FUZZY_F_H,         PINYIN_AmbFoHe         },
    { PINYIN_FUZZY_H_F,         PINYIN_AmbHeFo         },
    { PINYIN_FUZZY_L_R,         PINYIN_AmbLeRi         },
    { PINYIN_FUZZY_R_L,         PINYIN_AmbRiLe         },
    { PINYIN_FUZZY_K_G,         PINYIN_AmbKeGe         },
    { PINYIN_FUZZY_G_K,         PINYIN_AmbGeKe         },
    { PINYIN_FUZZY_AN_ANG,      PINYIN_AmbAnAng        },
    { PINYIN_FUZZY_ANG_AN,      PINYIN_AmbAngAn        },
    { PINYIN_FUZZY_EN_ENG,      PINYIN_AmbEnEng        },
    { PINYIN_FUZZY_ENG_EN,      PINYIN_AmbEngEn        },
    { PINYIN_FUZZY_IN_ING,      PINYIN_AmbInIng        },
    { PINYIN_FUZZY_ING_IN,      PINYIN_AmbIngIn        }
};


gboolean
LibPinyinBackEnd::setFuzzyOptions (Config *config, pinyin_context_t *context)
{
    g_assert (context);

    guint option = config->option ();
    PinyinCustomSettings custom;

    custom.set_use_incomplete (option & PINYIN_INCOMPLETE_PINYIN);
    custom.set_use_ambiguities (PINYIN_AmbAny, false);

    /* copy values */
    for (guint i = 0; i < G_N_ELEMENTS (fuzzy_options); i++) {
        if ( option & fuzzy_options[i].ibus_pinyin_option )
            custom.set_use_ambiguities
                (fuzzy_options[i].libpinyin_option, true);
    }

    pinyin_set_options(context, &custom);

    return TRUE;
}

/* Here are the double pinyin keyboard scheme mapping table. */
static const struct{
    gint double_pinyin_keyboard;
    PinyinShuangPinScheme shuang_pin_keyboard;
} shuang_pin_options [] = {
    {0, SHUANG_PIN_MS},
    {1, SHUANG_PIN_ZRM},
    {2, SHUANG_PIN_ABC},
    {3, SHUANG_PIN_ZIGUANG},
    {4, SHUANG_PIN_PYJJ},
    {5, SHUANG_PIN_XHE}
};

gboolean
LibPinyinBackEnd::setPinyinOptions (Config *config)
{
    if (NULL == m_pinyin_context)
        return FALSE;

    const gint map = config->doublePinyinSchema ();
    for (guint i = 0; i < G_N_ELEMENTS (shuang_pin_options); i++) {
        if (map == shuang_pin_options[i].double_pinyin_keyboard) {
            /* TODO: set double pinyin scheme. */
            PinyinShuangPinScheme scheme = shuang_pin_options[i].shuang_pin_keyboard;
            pinyin_set_double_pinyin_scheme (m_pinyin_context, scheme);
        }
    }

    setFuzzyOptions (config, m_pinyin_context);
    return TRUE;
}

/* Here are the chewing keyboard scheme mapping table. */
static const struct {
    gint bopomofo_keyboard;
    PinyinZhuYinScheme chewing_keyboard;
} chewing_options [] = {
    {0, ZHUYIN_STANDARD},
    {1, ZHUYIN_GIN_YIEH},
    {2, ZHUYIN_ET26},
    {3, ZHUYIN_IBM}
};


gboolean
LibPinyinBackEnd::setChewingOptions (Config *config)
{
    if (NULL == m_chewing_context)
        return FALSE;

    const gint map = config->bopomofoKeyboardMapping ();
    for (guint i = 0; i < G_N_ELEMENTS (chewing_options); i++) {
        if (map == chewing_options[i].bopomofo_keyboard) {
            /* TODO: set chewing scheme. */
            PinyinZhuYinScheme scheme = chewing_options[i].chewing_keyboard;
            pinyin_set_chewing_scheme (m_chewing_context, scheme);
        }
    }

    setFuzzyOptions (config, m_chewing_context);
    return TRUE;
}

void
LibPinyinBackEnd::modified (void)
{
    /* Restart the timer */
    g_timer_start (m_timer);

    if (m_timeout_id != 0)
        return;

    m_timeout_id = g_timeout_add_seconds (LIBPINYIN_SAVE_TIMEOUT,
                                          LibPinyinBackEnd::timeoutCallback,
                                          static_cast<gpointer> (this));
}

gboolean
LibPinyinBackEnd::timeoutCallback (gpointer data)
{
    LibPinyinBackEnd *self = static_cast<LibPinyinBackEnd *> (data);

    /* Get the elapsed time since last modification of database. */
    guint elapsed = (guint)g_timer_elapsed (self->m_timer, NULL);

    if (elapsed >= LIBPINYIN_SAVE_TIMEOUT &&
        self->saveUserDB ()) {
        self->m_timeout_id = 0;
        return FALSE;
    }

    return TRUE;
}

gboolean
LibPinyinBackEnd::saveUserDB (void)
{
    if (m_pinyin_context)
        pinyin_save (m_pinyin_context);
    if (m_chewing_context)
        pinyin_save (m_chewing_context);
    return TRUE;
}
