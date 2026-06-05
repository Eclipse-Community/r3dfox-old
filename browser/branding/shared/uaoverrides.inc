#define GUAO_PREF general.useragent.override

#define UXP_VERSION @MOZILLA_UAVERSION_U@
#define UXP_VERSION_SLICE Goanna/@UXP_VERSION@
#define UXP_DATE_SLICE Goanna/20170101
#define APP_SLICE Basilisk/@MOZ_APP_VERSION@

#define GK_VERSION 52.9
#define GK_SLICE Gecko/20100101
#define FX_SLICE Firefox/@GK_VERSION@

// Pale Moon Version needed for challenges.cloudflare.com user agent override
#define PM_VERSION 33.6.1
#define PM_SLICE PaleMoon/@PM_VERSION@

// %OS_SLICE% macro is resolved at runtime, see MoonchildProductions/UXP/issues/1473

// Special-case AMO
// We send the native UA slice now, since they no longer offer any compatible extensions for us.
// This will result in an "only with Firefox" message which suits us fine, because it's the truth.
pref("@GUAO_PREF@.addons.mozilla.org","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");

// Required for domains that are unresponsive to requests from users (or likely to be)
pref("@GUAO_PREF@.aol.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ (Basilisk)");
pref("@GUAO_PREF@.bing.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ (Basilisk)");
pref("@GUAO_PREF@.canva.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ (Pale Moon)");
pref("@GUAO_PREF@.chase.com","Mozilla/5.0 (%OS_SLICE% rv:102.0) @GK_SLICE@ Firefox/102.0");
pref("@GUAO_PREF@.dropbox.com","Mozilla/5.0 (%OS_SLICE% rv:68.9) @GK_SLICE@ Firefox/68.9 (Basilisk)");
pref("@GUAO_PREF@.kroger.com","Mozilla/5.0 (%OS_SLICE% rv:86.0) @GK_SLICE@ Firefox/86.0 (Basilisk)");
pref("@GUAO_PREF@.netteller.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.patientaccess.com","Mozilla/5.0 (%OS_SLICE% rv:60.0) @GK_SLICE@ Firefox/60.0 @APP_SLICE@");
pref("@GUAO_PREF@.outlook.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ (Basilisk)");
pref("@GUAO_PREF@.web.de","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ (Basilisk)");
pref("@GUAO_PREF@.yahoo.com","Mozilla/5.0 (%OS_SLICE% rv:99.9) @GK_SLICE@ Firefox/99.9");
pref("@GUAO_PREF@.calendar.yahoo.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ (Basilisk)");

// Soundcloud uses Firefox-exclusive combinations of code. Never pass Firefox slice.
pref("@GUAO_PREF@.soundcloud.com","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");
pref("@GUAO_PREF@.players.brightcove.net","Mozilla/5.0 (Windows NT 6.1; Trident/7.0; rv:11.0) like Gecko");
// Google fonts serves physically different fonts to later Firefox versions that render incorrectly unless on Gecko
pref("@GUAO_PREF@.fonts.googleapis.com", "Mozilla/5.0 (%OS_SLICE% rv:61.9) Gecko/20100101 Firefox/61.9");
pref("@GUAO_PREF@.fonts-api.wp.com", "Mozilla/5.0 (%OS_SLICE% rv:61.9) Gecko/20100101 Firefox/61.9");

// The following requires native mode. Or it blocks.. "too old firefox", breakage, etc.
pref("@GUAO_PREF@.deviantart.com","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");
pref("@GUAO_PREF@.deviantart.net","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");
pref("@GUAO_PREF@.altibox.dk","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");
pref("@GUAO_PREF@.altibox.no","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");
pref("@GUAO_PREF@.mozilla.org","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");
pref("@GUAO_PREF@.mozilla.com","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");
pref("@GUAO_PREF@.github.com","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");
pref("@GUAO_PREF@.zoho.com","Mozilla/5.0 (%OS_SLICE% rv:@UXP_VERSION@) @UXP_DATE_SLICE@ @APP_SLICE@");

// UA-Sniffing domains below have indicated no interest in supporting Basilisk (BOO!)
pref("@GUAO_PREF@.humblebundle.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ (Basilisk)");
pref("@GUAO_PREF@.privat24.ua","Mozilla/5.0 (%OS_SLICE% rv:38.0) @GK_SLICE@ Firefox/38.0");
pref("@GUAO_PREF@.citi.com","Mozilla/5.0 (%OS_SLICE% rv:68.0) @GK_SLICE@ Firefox/68.0 SeaMonkey/2.53.12");
pref("@GUAO_PREF@.facebook.com","Mozilla/5.0 (%OS_SLICE% rv:68.0) @GK_SLICE@ Firefox/68.0 @APP_SLICE@");
pref("@GUAO_PREF@.mewe.com", "Mozilla/5.0 (%OS_SLICE% rv:102.0) Gecko/20100101 Firefox/102.0");

// UA-sniffing domains that are "app/vendor-specific" and do not like Basilisk
pref("@GUAO_PREF@.web.whatsapp.com","Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/70.0.3538.77 Safari/537.36");
pref("@GUAO_PREF@.youtube.com","Mozilla/5.0 (%OS_SLICE% rv:102.0) @GK_SLICE@ Firefox/102.0");
pref("@GUAO_PREF@.studio.youtube.com","Mozilla/5.0 (%OS_SLICE% rv:102.0) @GK_SLICE@ @UXP_VERSION_SLICE@ Firefox/102.0 @APP_SLICE@");

// The following domains do not like the Goanna slice
pref("@GUAO_PREF@.bab.la","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.babla.gr","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.collinsdictionary.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.dictionary.cambridge.org","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.hitbox.tv","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.ldoceonline.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@");
pref("@GUAO_PREF@.yuku.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @FX_SLICE@ @APP_SLICE@");

// Domains Basilisk overrides that are not in the Pale Moon overrides
pref("@GUAO_PREF@.slack.com","Mozilla/5.0 (%OS_SLICE% rv:140.0) @GK_SLICE@ Firefox/140.0 @APP_SLICE@");

// Domains that require a Pale Moon user agent override
//pref("@GUAO_PREF@.challenges.cloudflare.com","Mozilla/5.0 (%OS_SLICE% rv:@GK_VERSION@) @GK_SLICE@ @UXP_VERSION_SLICE@ @FX_SLICE@ @PM_SLICE@");

// ============================================================================