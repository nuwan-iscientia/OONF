include $(TOPDIR)/rules.mk

PKG_NAME:=oonf-init-scripts
PKG_VERSION:=2015-06-08
#PKG_REV:=ec2bca8f28b4d14df049fc81e177160ca0e3c3a3
#PKG_RELEASE:=$(PKG_REV)

#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.bz2
#PKG_SOURCE_URL:=http://olsr.org/git/oonf_api.git
#PKG_SOURCE_PROTO:=git
#PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
#PKG_SOURCE_VERSION:=$(PKG_REV)

include $(INCLUDE_DIR)/package.mk

define Package/oonf-init-scripts
	SECTION:=net
	CATEGORY:=Network
	MAINTAINER:=Henning Rogge <hrogge@gmail.com>
	SUBMENU:=OLSR.org network framework
	URL:=http://www.olsr.org/
	TITLE:= Common OONF startup scripts
	VERSION:=$(PKG_VERSION)
endef

define Package/oonf-olsrd2-git/install
	$(INSTALL_BIN)  -D ./files/oonf_init.sh            $(1)/lib/functions/oonf_init.sh
	$(INSTALL_BIN)  -D ./files/oonf_hotplug.sh         $(1)/lib/functions/oonf_hotplug.sh
endef

$(eval $(call BuildPackage,oonf-init-scripts))
