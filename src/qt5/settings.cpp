#include <QSettings>

#include "rpcemu.h"


/**
 * Load the user's previous chosen configuration. Will fill in sensible
 * defaults if any configuration values are absent.
 *
 * Called on program startup.
 *
 * @param config
 */
void
config_load(Config * config)
{


	char filename[512];
	const char *p;
	Model model;
	int i;
	QString sText;
	QByteArray ba;

	snprintf(filename, sizeof(filename), "%srpc.cfg", rpcemu_get_datadir());
//	set_config_file(filename);

	QSettings settings("rpc.cfg", QSettings::IniFormat);


	/* Copy the contents of the configfile to the log */
/*
	{
		const char **entries = NULL;
		int n = list_config_entries(NULL, &entries);
		int i;

		for (i = 0; i < n; i++) {
			rpclog("loadconfig: %s = \"%s\"\n", entries[i],
			       get_config_string(NULL, entries[i], "-"));
		}
		free_config_entries(&entries);
	}
*/

	sText = settings.value("mem_size", "16").toString();
	ba = sText.toUtf8();
        p = ba.data();
/*	p = get_config_string(NULL, "mem_size", NULL);
	if (p == NULL) {
		config.mem_size = 16;
	} else */
	if (!strcmp(p, "4")) {
		config->mem_size = 4;
	} else if (!strcmp(p, "8")) {
		config->mem_size = 8;
	} else if (!strcmp(p, "32")) {
		config->mem_size = 32;
	} else if (!strcmp(p, "64")) {
		config->mem_size = 64;
	} else if (!strcmp(p, "128")) {
		config->mem_size = 128;
	} else if (!strcmp(p, "256")) {
		config->mem_size = 256;
	} else {
		config->mem_size = 16;
	}

	sText = settings.value("vram_size", "").toString();
	if (!QString::compare(sText, "", Qt::CaseInsensitive)) {
		config->vrammask = 0x7FFFFF;
	} else if (!QString::compare(sText, "0", Qt::CaseInsensitive)) {
		config->vrammask = 0;
	} else {
		config->vrammask = 0x7FFFFF;
	}

	sText = settings.value("model", "").toString();
	ba = sText.toUtf8();
        p = ba.data();
	model = Model_RPCARM710;
	if (p != NULL) {
		for (i = 0; i < Model_MAX; i++) {
			if (strcasecmp(p, models[i].name_config) == 0) {
				model = (Model) i;
				break;
			}
		}
	}

	rpcemu_model_changed(model);

	/* A7000 and A7000+ have no VRAM */
	if (model == Model_A7000 || model == Model_A7000plus) {
		config->vrammask = 0;
	}

	/* If Phoebe, override some settings */
	if (model == Model_Phoebe) {
		config->mem_size = 256;
		config->vrammask = 0x3fffff;
	}

	config->soundenabled = settings.value("sound_enabled", "1").toInt();
	config->refresh      = settings.value("refresh_rate", "60").toInt();
	config->cdromenabled = settings.value("cdrom_enabled", "0").toInt();
	config->cdromtype    = settings.value("cdrom_type", "0").toInt();

	sText = settings.value("cdrom_iso", "").toString();
	ba = sText.toUtf8();
        p = ba.data();
        if (!p) strcpy(config->isoname, "");
        else    strcpy(config->isoname, p);

	config->mousehackon = settings.value("mouse_following", "1").toInt();
	config->mousetwobutton = settings.value("mouse_twobutton", "0").toInt();

	sText = settings.value("network_type", "off").toString();
	if (!QString::compare(sText, "off", Qt::CaseInsensitive)) {
		config->network_type = NetworkType_Off;
	} else if (!QString::compare(sText, "iptunnelling", Qt::CaseInsensitive)) {
		config->network_type = NetworkType_IPTunnelling;
	} else if (!QString::compare(sText, "ethernetbridging", Qt::CaseInsensitive)) {
		config->network_type = NetworkType_EthernetBridging;
	} else {
		QByteArray ba = sText.toUtf8();
		rpclog("Unknown network_type '%s', defaulting to off\n", ba.data());
		config->network_type = NetworkType_Off;
	}

	/* Take a copy of the string config values, to allow dynamic alteration
	   later */
	sText = settings.value("username", "").toString();
	ba = sText.toUtf8();
	config->username = strdup(ba.data());

	sText = settings.value("ipaddress", "").toString();
	ba = sText.toUtf8();
	config->ipaddress = strdup(ba.data());

	sText = settings.value("macaddress", "").toString();
	ba = sText.toUtf8();
	config->macaddress = strdup(ba.data());

	sText = settings.value("bridgename", "").toString();
	ba = sText.toUtf8();
	config->bridgename = strdup(ba.data());

	config->cpu_idle = 0;
	config->cpu_idle = settings.value("cpu_idle", "0").toInt();
}


/**
 * Store the user's most recently chosen configuration to disc, for use next
 * time the program starts.
 *
 * Called on program exit.
 */
void
config_save(Config *config)
{
	char filename[512];
	QString sText;

	snprintf(filename, sizeof(filename), "%srpc.cfg", rpcemu_get_datadir());

	QSettings settings("rpc.cfg", QSettings::IniFormat);

        char s[256];

	sprintf(s, "%u", config->mem_size);
//	set_config_string(NULL, "mem_size", s);
	settings.setValue("mem_size", s);

	sprintf(s, "%s", models[machine.model].name_config);
//	set_config_string(NULL, "model", s);
	settings.setValue("model", s);

        if (config->vrammask) settings.setValue("vram_size", "2");
        else                 settings.setValue("vram_size", "0");

	settings.setValue("sound_enabled",   config->soundenabled);
	settings.setValue("refresh_rate",    config->refresh);
	settings.setValue("cdrom_enabled",   config->cdromenabled);
	settings.setValue("cdrom_type",      config->cdromtype);
	settings.setValue("cdrom_iso",       config->isoname);
	settings.setValue("mouse_following", config->mousehackon);
	settings.setValue("mouse_twobutton", config->mousetwobutton);


	switch (config->network_type) {
	case NetworkType_Off:              sprintf(s, "off"); break;
	case NetworkType_EthernetBridging: sprintf(s, "ethernetbridging"); break;
	case NetworkType_IPTunnelling:     sprintf(s, "iptunnelling"); break;
	default:
		/* Forgotten to add a new network type to the switch()? */
		fatal("saveconfig(): unknown networktype %d\n",
		      config->network_type);
	}
	settings.setValue("network_type", s);

	if (config->username) {
		settings.setValue("username", config->username);
	} else {
		settings.setValue("username", "");
	}
	if (config->ipaddress) {
		settings.setValue("ipaddress", config->ipaddress);
	} else {
		settings.setValue("ipaddress", "");
	}
	if (config->macaddress) {
		settings.setValue("macaddress", config->macaddress);
	} else {
		settings.setValue("macaddress", "");
	}
	if (config->bridgename) {
		settings.setValue("bridgename", config->bridgename);
	} else {
		settings.setValue("bridgename", "");
	}

	settings.setValue("cpu_idle", config->cpu_idle);
}
