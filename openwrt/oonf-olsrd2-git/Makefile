include $(TOPDIR)/rules.mk

PKG_NAME:=oonf-git
PKG_VERSION:=2014-11-07
#PKG_REV:=ec2bca8f28b4d14df049fc81e177160ca0e3c3a3
#PKG_RELEASE:=$(PKG_REV)

#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
#PKG_SOURCE_URL:=http://olsr.org/git/oonf_api.git
#PKG_SOURCE_PROTO:=git
#PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
#PKG_SOURCE_VERSION:=$(PKG_REV)

CMAKE_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

CMAKE_SOURCE_DIR=$(CURDIR)/../../
CMAKE_OPTIONS=-D OONF_NO_WERROR:Bool=true \
              -D OONF_LOGGING_LEVEL:String=debug \
              -D OONF_NO_TESTING:Bool=true \
              -D UCI:Bool=true \
              -D OONF_APP_DEFAULT_CFG_HANDLER:String=uci \
              -D OONF_STATIC_PLUGINS:String="class;clock;duplicate_set;interface;layer2;packet_socket;rfc5444;socket;stream_socket;telnet;timer;viewer;os_clock;os_net;os_routing;os_system;nhdp;olsrv2;ff_dat_metric;neighbor_probing;nl80211_listener;link_config;layer2info;systeminfo;cfg_uciloader;nhdpinfo;olsrv2info" \
              -D INSTALL_LIB_DIR:Path=lib/oonf \
              -D INSTALL_INCLUDE_DIR:Path=include/oonf \
              -D INSTALL_CMAKE_DIR:Path=lib/oonf \
              -D CMAKE_PREFIX_PATH=$(STAGING_DIR)/usr

define Package/oonf-git/template
	SECTION:=net
	CATEGORY:=Network
	MAINTAINER:=Henning Rogge <hrogge@gmail.com>
	SUBMENU:=OLSR.org network framework
	URL:=http://www.olsr.org/
endef

define Package/olsrd2-git
	$(call Package/oonf-git/template) 
	TITLE:= Build Olsrd V2 Routing Agent
	DEPENDS:=+librt +libnl-tiny +libuci
endef

Build/Compile=$(call Build/Compile/Default,static)
Build/Install=

define Build/Install
	$(INSTALL_BIN) -D $(PKG_BUILD_DIR)/$(MAKE_PATH)/olsrd2_static $(PKG_INSTALL_DIR)/usr/sbin/olsrd2;
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include -I${STAGING_DIR}/usr/include/libnl-tiny

define Package/olsrd2-git/install
	$(INSTALL_BIN)  -D $(PKG_BUILD_DIR)/olsrd2_static $(1)/usr/sbin/olsrd2
	$(INSTALL_BIN)  -D ./files/olsrd2.init            $(1)/etc/init.d/olsrd2
	$(INSTALL_BIN)  -D ./files/olsrd2.hotplug         $(1)/etc/hotplug.d/iface/50-olsrd2
	$(INSTALL_DATA) -D ./files/olsrd2.uci             $(1)/etc/config/olsrd2
endef

define Package/olsrd2-git/conffiles
/etc/config/olsrd2
endef

$(eval $(call BuildPackage,olsrd2-git))
