# the main Texinfo document (the manual)
info_TEXINFOS = wendy.texi

# any other files you need to add (images)
EXTRA_DIST = toolpaper.pdf extra_man

# the height of the README's ASCII art + URL + 3 spaces
BANNERHEIGHT = 10

# the help option that should be used in the manual
HELPOPTION = --detailed-help


#############################################################################
# EVERYTHING BELOW THIS LINE IS GENERIC - YOU MUST NOT CHANGE ANYTHING BELOW
#############################################################################

#--------------------------------------#
# GENERIC: add FDL and helper TeX file #
#--------------------------------------#
wendy_TEXINFOS = fdl.texi

#-----------------------------------------------------------#
# GENERIC: run tool to include help output in documentation #
#-----------------------------------------------------------#
cmdline.out: $(top_srcdir)/src/cmdline.ggo $(top_srcdir)/configure.ac
	$(AM_V_GEN)$(top_builddir)/src/$(PACKAGE)$(EXEEXT) $(HELPOPTION) > cmdline.out
EXTRA_DIST += cmdline.out 
MAINTAINERCLEANFILES = cmdline.out
wendy_TEXINFOS += cmdline.out

#-----------------------------------------------------------#
# GENERIC: create ASCII files out README and Texinfo manual #
#-----------------------------------------------------------#
all: $(top_srcdir)/ChangeLog $(top_srcdir)/REQUIREMENTS $(top_srcdir)/THEORY
wendy_TEXINFOS += ChangeLog.texi
EXTRA_DIST += $(top_srcdir)/REQUIREMENTS $(top_srcdir)/THEORY

$(top_srcdir)/ChangeLog: ChangeLog.texi
	$(AM_V_GEN)
	$(AM_V_at)$(MAKEINFO) -o ChangeLog.txt --no-split --no-headers --fill-column=79 $(srcdir)/$(PACKAGE).texi
	$(AM_V_at)$(SED) -n '/procrastination/,/The most recent change log/w ChangeLog2.txt' ChangeLog.txt
	$(AM_V_at)$(SED) '1,2d' ChangeLog2.txt | $(SED) -e '$$d' | $(SED) -e '$$d' | $(SED) 's/^   //' > ChangeLog.txt
	$(AM_V_at)rm ChangeLog2.txt
	$(AM_V_at)head -n$(BANNERHEIGHT) $(top_srcdir)/README > $(top_srcdir)/ChangeLog
	$(AM_V_at)cat ChangeLog.txt >> $(top_srcdir)/ChangeLog
	$(AM_V_at)head -n$(BANNERHEIGHT) $(top_srcdir)/README > $(top_srcdir)/NEWS
	$(AM_V_at)head -n `($(GREP) -n -m2 "^Version " ChangeLog.txt | $(SED) -n '2p' | $(SED) -e 's/:/ /' | $(AWK) '{print $$1-1}')` ChangeLog.txt >> $(top_srcdir)/NEWS
	$(AM_V_at)echo "\nSee file ChangeLog <http://service-technology.org/files/$(PACKAGE)/ChangeLog> for\ncomplete version history.\n" >> $(top_srcdir)/NEWS
	$(AM_V_at)rm ChangeLog.txt

$(top_srcdir)/REQUIREMENTS: $(PACKAGE).texi
	$(AM_V_GEN)
	$(AM_V_at)$(MAKEINFO) -o ChangeLog.txt --no-split --no-headers --no-number-sections --fill-column=79 $(srcdir)/$(PACKAGE).texi
	$(AM_V_at)$(SED) -n '/In order to run, compile, and/,/development./w ChangeLog2.txt' ChangeLog.txt
	$(AM_V_at)head -n$(BANNERHEIGHT) $(top_srcdir)/README > $(top_srcdir)/REQUIREMENTS
	$(AM_V_at)cat ChangeLog2.txt >> $(top_srcdir)/REQUIREMENTS
	$(AM_V_at)echo "" >> $(top_srcdir)/REQUIREMENTS
	$(AM_V_at)rm ChangeLog.txt ChangeLog2.txt

$(top_srcdir)/THEORY: $(PACKAGE).texi
	$(AM_V_GEN)
	$(AM_V_at)$(MAKEINFO) -o THEORY.txt --no-split --no-headers --no-number-sections --fill-column=79 $(srcdir)/$(PACKAGE).texi
	$(AM_V_at)$(SED) -n '/$(PACKAGE_NAME) implements several algorithms/,/for more information./w THEORY2.txt' THEORY.txt
	$(AM_V_at)head -n$(BANNERHEIGHT) $(top_srcdir)/README > $(top_srcdir)/THEORY
	$(AM_V_at)cat THEORY2.txt >> $(top_srcdir)/THEORY
	$(AM_V_at)echo "" >> $(top_srcdir)/THEORY
	$(AM_V_at)rm THEORY.txt THEORY2.txt


#---------------------------------------#
# GENERIC: run tool to create a manpage #
#---------------------------------------#

man_MANS = $(PACKAGE).1

MAINTAINERCLEANFILES += $(man_MANS) $(man_MANS:.1=)-man.pdf
EXTRA_DIST += $(man_MANS)

$(PACKAGE).1: $(top_srcdir)/configure.ac $(top_srcdir)/src/cmdline.ggo extra_man
	$(AM_V_GEN)$(HELP2MAN) \
		$(top_builddir)/src/$(PACKAGE)$(EXEEXT) \
		--name="synthesizing partners for services" \
		--output=$@ \
		--manual="service-technology.org" \
		--source="service-technology.org" \
		--help-option="$(HELPOPTION)" \
		--opt-include=extra_man

man-pdf: $(man_MANS:.1=)-man.pdf

%-man.pdf: %.1
	$(AM_V_GEN)groff -Tps -mandoc -c $< | ps2pdf - $(@:.1=)


#-------------------------------------------------------------------#
# run the "gendocs.sh" script to create a manual in several formats
#-------------------------------------------------------------------#
EXTRA_DIST += gendocs.sh gendocs_template

manual: $(info_TEXINFOS)
	./gendocs.sh --email $(PACKAGE_BUGREPORT) $(PACKAGE) "$(PACKAGE_NAME)"
	-cp -R figs manual
	-cp -R figs manual/html_node
	./gendocs.sh $(PACKAGE) "$(PACKAGE_NAME)"
