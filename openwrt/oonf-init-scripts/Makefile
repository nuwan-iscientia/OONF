include $(TOPDIR)/rules.mk

PKG_NAME:=oonf-init-scripts
PKG_VERSION:=2015-06-08

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

define Package/oonf-init-scripts/install
	$(INSTALL_BIN)  -D ./files/oonf_init.sh            $(1)/lib/functions/oonf_init.sh
	$(INSTALL_BIN)  -D ./files/oonf_hotplug.sh         $(1)/lib/functions/oonf_hotplug.sh
endef

$(eval $(call BuildPackage,oonf-init-scripts))
