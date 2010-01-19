// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

#include <gtk/gtk.h>

#include <vector>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Style for dialog group titles.
const char kDialogGroupTitleMarkup[] = "<span weight='bold'>%s</span>";

// How far we indent dialog widgets, in pixels.
const int kAutoFillDialogIndent = 5;

// Adds an alignment around |widget| which indents the widget by |offset|.
GtkWidget* IndentWidget(GtkWidget* widget, int offset) {
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment), 0, 0,
                            offset, 0);
  gtk_container_add(GTK_CONTAINER(alignment), widget);
  return alignment;
}

// Makes sure we use the gtk theme colors by loading the base color of an entry
// widget.
void SetWhiteBackground(GtkWidget* widget) {
  GtkWidget* entry = gtk_entry_new();
  gtk_widget_ensure_style(entry);
  GtkStyle* style = gtk_widget_get_style(entry);
  gtk_widget_modify_bg(widget, GTK_STATE_NORMAL,
                       &style->base[GTK_STATE_NORMAL]);
  gtk_widget_destroy(entry);
}

////////////////////////////////////////////////////////////////////////////////
// Form Table helpers.
//
// The following functions can be used to create a form with labeled widgets.
//

// Creates a form table with dimensions |rows| x |cols|.
GtkWidget* InitFormTable(int rows, int cols) {
  // We have two table rows per form table row.
  GtkWidget* table = gtk_table_new(rows * 2, cols, false);
  gtk_table_set_row_spacings(GTK_TABLE(table), gtk_util::kControlSpacing);
  gtk_table_set_col_spacings(GTK_TABLE(table), gtk_util::kFormControlSpacing);

  // Leave no space between the label and the widget.
  for (int i = 0; i < rows; i++)
    gtk_table_set_row_spacing(GTK_TABLE(table), i * 2, 0);

  return table;
}

// Sets the label of the form widget at |row|,|col|.  The label is |len| columns
// long.
void FormTableSetLabel(
    GtkWidget* table, int row, int col, int len, int label_id) {
  // We have two table rows per form table row.
  row *= 2;

  const char* text =
      (label_id) ? l10n_util::GetStringUTF8(label_id).c_str() : 0;
  GtkWidget* label = gtk_label_new(text);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_table_attach(GTK_TABLE(table), label,
                   col, col + len,  // Left col, right col.
                   row, row + 1,  // Top row, bottom row.
                   GTK_FILL, GTK_FILL,  // Options.
                   0, 0);  // Padding.
}

// Sets the form widget at |row|,|col|.  The widget fills up |len| columns.  If
// |expand| is true, the widget will expand to fill all of the extra space in
// the table row.
void FormTableSetWidget(GtkWidget* table,
                        GtkWidget* widget,
                        int row, int col,
                        int len, bool expand) {
  const GtkAttachOptions expand_option =
      static_cast<GtkAttachOptions>(GTK_FILL | GTK_EXPAND);
  GtkAttachOptions xoption = (expand) ?  expand_option : GTK_FILL;

  // We have two table rows per form table row.
  row *= 2;
  gtk_table_attach(GTK_TABLE(table), widget,
                   col, col + len,  // Left col, right col.
                   row + 1, row + 2,  // Top row, bottom row.
                   xoption, GTK_FILL,  // Options.
                   0, 0);  // Padding.
}

// Adds a labeled entry box to the form table at |row|,|col|.  The entry widget
// fills up |len| columns.  The returned widget is owned by |table| and should
// not be destroyed.
GtkWidget* FormTableAddEntry(
    GtkWidget* table, int row, int col, int len, int label_id) {
  FormTableSetLabel(table, row, col, len, label_id);

  GtkWidget* entry = gtk_entry_new();
  FormTableSetWidget(table, entry, row, col, len, false);

  return entry;
}

// Adds a labeled entry box to the form table that will expand to fill extra
// space in the table row.
GtkWidget* FormTableAddExpandedEntry(
    GtkWidget* table, int row, int col, int len, int label_id) {
  FormTableSetLabel(table, row, col, len, label_id);

  GtkWidget* entry = gtk_entry_new();
  FormTableSetWidget(table, entry, row, col, len, true);

  return entry;
}

// Adds a sized entry box to the form table.  The entry widget width is set to
// |char_len|.
void FormTableAddSizedEntry(
    GtkWidget* table, int row, int col, int char_len, int label_id) {
  GtkWidget* entry = FormTableAddEntry(table, row, col, 1, label_id);
  gtk_entry_set_width_chars(GTK_ENTRY(entry), char_len);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AutoFillDialog
//
// The contents of the AutoFill dialog.  This dialog allows users to add, edit
// and remove AutoFill profiles.
class AutoFillDialog {
 public:
  AutoFillDialog(std::vector<AutoFillProfile>* profiles,
                 std::vector<FormGroup>* credit_cards);
  ~AutoFillDialog() {}

  // Shows the AutoFill dialog.
  void Show();

 private:
  // 'destroy' signal handler.  We DeleteSoon the global singleton dialog object
  // from here.
  static void OnDestroy(GtkWidget* widget, AutoFillDialog* autofill_dialog);

  // 'clicked' signal handler.  We add a new address.
  static void OnAddAddressClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler.  We add a new credit card.
  static void OnAddCreditCardClicked(GtkButton* button, AutoFillDialog* dialog);

  // Initializes the group widgets and returns their container.  |name_id| is
  // the resource ID of the group label.  |button_id| is the resource name of
  // the button label.  |clicked_callback| is a callback that handles the
  // 'clicked' signal emitted when the user presses the 'Add' button.
  GtkWidget* InitGroup(int label_id,
                       int button_id,
                       GCallback clicked_callback);

  // Initializes the expander, frame and table widgets used to hold the address
  // and credit card forms.  |name_id| is the resource id of the label of the
  // expander widget.  The content vbox widget is returned in |content_vbox|.
  // Returns the expander widget.
  GtkWidget* InitGroupContentArea(int name_id, GtkWidget** content_vbox);

  // Returns a GtkExpander that is added to the appropriate vbox.  Each method
  // adds the necessary widgets and layout required to fill out information
  // for either an address or a credit card.
  GtkWidget* AddNewAddress();
  GtkWidget* AddNewCreditCard();

  // The list of current AutoFill profiles.  Owned by AutoFillManager.
  std::vector<AutoFillProfile>* profiles_;

  // The list of current AutoFill credit cards.  Owned by AutoFillManager.
  std::vector<FormGroup>* credit_cards_;

  // The AutoFill dialog.
  GtkWidget* dialog_;

  // The addresses group.
  GtkWidget* addresses_vbox_;

  // The credit cards group.
  GtkWidget* creditcards_vbox_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillDialog);
};

// The singleton AutoFill dialog object.
static AutoFillDialog* dialog = NULL;

AutoFillDialog::AutoFillDialog(std::vector<AutoFillProfile>* profiles,
                               std::vector<FormGroup>* credit_cards)
    : profiles_(profiles),
      credit_cards_(credit_cards) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_TITLE).c_str(),
      // AutoFill dialog is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_APPLY,
      GTK_RESPONSE_APPLY,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      NULL);

  gtk_widget_realize(dialog_);
  gtk_util::SetWindowSizeFromResources(GTK_WINDOW(dialog_),
                                       IDS_AUTOFILL_DIALOG_WIDTH_CHARS,
                                       IDS_AUTOFILL_DIALOG_HEIGHT_LINES,
                                       true);

  // Allow browser windows to go in front of the AutoFill dialog in Metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);

  // Allow the contents to be scrolled.
  GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), scrolled_window);

  // We create an event box so that we can color the frame background white.
  GtkWidget* frame_event_box = gtk_event_box_new();
  SetWhiteBackground(frame_event_box);
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window),
                                        frame_event_box);

  // The frame outline of the content area.
  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_container_add(GTK_CONTAINER(frame_event_box), frame);

  // The content vbox.
  GtkWidget* outer_vbox = gtk_vbox_new(false, 0);
  gtk_box_set_spacing(GTK_BOX(outer_vbox), gtk_util::kContentAreaSpacing);
  gtk_container_add(GTK_CONTAINER(frame), outer_vbox);

  addresses_vbox_ = InitGroup(IDS_AUTOFILL_ADDRESSES_GROUP_NAME,
                              IDS_AUTOFILL_ADD_ADDRESS_BUTTON,
                              G_CALLBACK(OnAddAddressClicked));
  gtk_box_pack_start_defaults(GTK_BOX(outer_vbox), addresses_vbox_);

  // TODO(jhawkins): Add addresses from |profiles|.

  creditcards_vbox_ = InitGroup(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME,
                                IDS_AUTOFILL_ADD_CREDITCARD_BUTTON,
                                G_CALLBACK(OnAddCreditCardClicked));
  gtk_box_pack_start_defaults(GTK_BOX(outer_vbox), creditcards_vbox_);

  // TODO(jhawkins): Add credit cards from |credit_cards|.

  gtk_widget_show_all(dialog_);
}

void AutoFillDialog::Show() {
  gtk_window_present_with_time(GTK_WINDOW(dialog_),
                               gtk_get_current_event_time());
}

// static
void AutoFillDialog::OnDestroy(GtkWidget* widget,
                               AutoFillDialog* autofill_dialog) {
  dialog = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, autofill_dialog);
}

// static
void AutoFillDialog::OnAddAddressClicked(GtkButton* button,
                                         AutoFillDialog* dialog) {
  GtkWidget* new_address = dialog->AddNewAddress();
  gtk_box_pack_start(GTK_BOX(dialog->addresses_vbox_), new_address,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(new_address);
}

// static
void AutoFillDialog::OnAddCreditCardClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  GtkWidget* new_creditcard = dialog->AddNewCreditCard();
  gtk_box_pack_start(GTK_BOX(dialog->creditcards_vbox_), new_creditcard,
                     FALSE, FALSE, 0);
  gtk_widget_show_all(new_creditcard);
}

GtkWidget* AutoFillDialog::InitGroup(int name_id,
                                     int button_id,
                                     GCallback clicked_callback) {
  GtkWidget* vbox = gtk_vbox_new(false, gtk_util::kControlSpacing);

  // Group label.
  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(
      kDialogGroupTitleMarkup,
      l10n_util::GetStringUTF8(name_id).c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_box_pack_start(GTK_BOX(vbox),
                     IndentWidget(label, kAutoFillDialogIndent),
                     FALSE, FALSE, 0);

  // Separator.
  GtkWidget* separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

  // Add profile button.
  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(button_id).c_str());
  g_signal_connect(button, "clicked", clicked_callback, this);
  gtk_box_pack_end_defaults(GTK_BOX(vbox),
                            IndentWidget(button, kAutoFillDialogIndent));

  return vbox;
}

GtkWidget* AutoFillDialog::InitGroupContentArea(int name_id,
                                                GtkWidget** content_vbox) {
  GtkWidget* expander = gtk_expander_new(
      l10n_util::GetStringUTF8(name_id).c_str());

  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_container_add(GTK_CONTAINER(expander), frame);

  GtkWidget* vbox = gtk_vbox_new(false, 0);
  gtk_box_set_spacing(GTK_BOX(vbox), gtk_util::kControlSpacing);
  GtkWidget* vbox_alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(vbox_alignment),
                            gtk_util::kControlSpacing,
                            gtk_util::kControlSpacing,
                            gtk_util::kGroupIndent,
                            0);
  gtk_container_add(GTK_CONTAINER(vbox_alignment), vbox);
  gtk_container_add(GTK_CONTAINER(frame), vbox_alignment);

  // Make it expand by default.
  gtk_expander_set_expanded(GTK_EXPANDER(expander), true);

  *content_vbox = vbox;
  return expander;
}

GtkWidget* AutoFillDialog::AddNewAddress() {
  GtkWidget* vbox;
  GtkWidget* address = InitGroupContentArea(IDS_AUTOFILL_NEW_ADDRESS, &vbox);

  GtkWidget* table = InitFormTable(5, 3);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), table);

  FormTableAddEntry(table, 0, 0, 1, IDS_AUTOFILL_DIALOG_LABEL);
  FormTableAddEntry(table, 1, 0, 1, IDS_AUTOFILL_DIALOG_FIRST_NAME);
  FormTableAddEntry(table, 1, 1, 1, IDS_AUTOFILL_DIALOG_MIDDLE_NAME);
  FormTableAddEntry(table, 1, 2, 1, IDS_AUTOFILL_DIALOG_LAST_NAME);
  FormTableAddEntry(table, 2, 0, 1, IDS_AUTOFILL_DIALOG_EMAIL);
  FormTableAddEntry(table, 2, 1, 1, IDS_AUTOFILL_DIALOG_COMPANY_NAME);
  FormTableAddEntry(table, 3, 0, 2, IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1);
  FormTableAddEntry(table, 4, 0, 2, IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2);

  // TODO(jhawkins): If there's not a default profile, automatically check this
  // check button.
  GtkWidget* default_check = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_MAKE_DEFAULT).c_str());
  FormTableSetWidget(table, default_check, 0, 1, 1, false);

  GtkWidget* address_table = InitFormTable(1, 4);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), address_table);

  FormTableAddEntry(address_table, 0, 0, 1, IDS_AUTOFILL_DIALOG_CITY);
  FormTableAddEntry(address_table, 0, 1, 1, IDS_AUTOFILL_DIALOG_STATE);
  FormTableAddSizedEntry(address_table, 0, 2, 7, IDS_AUTOFILL_DIALOG_ZIP_CODE);
  FormTableAddSizedEntry(address_table, 0, 3, 10, IDS_AUTOFILL_DIALOG_COUNTRY);

  GtkWidget* phone_table = InitFormTable(1, 8);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), phone_table);

  FormTableAddSizedEntry(phone_table, 0, 0, 4, IDS_AUTOFILL_DIALOG_PHONE);
  FormTableAddSizedEntry(phone_table, 0, 1, 4, 0);
  FormTableAddEntry(phone_table, 0, 2, 2, 0);
  FormTableAddSizedEntry(phone_table, 0, 4, 4, IDS_AUTOFILL_DIALOG_FAX);
  FormTableAddSizedEntry(phone_table, 0, 5, 4, 0);
  FormTableAddEntry(phone_table, 0, 6, 2, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), button);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), alignment);

  return address;
}

GtkWidget* AutoFillDialog::AddNewCreditCard() {
  GtkWidget* vbox;
  GtkWidget* credit_card = InitGroupContentArea(IDS_AUTOFILL_NEW_CREDITCARD,
                                                &vbox);

  GtkWidget* label_table = InitFormTable(1, 2);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), label_table);

  FormTableAddEntry(label_table, 0, 0, 1, IDS_AUTOFILL_DIALOG_LABEL);

  // TODO(jhawkins): If there's not a default profile, automatically check this
  // check button.
  GtkWidget* default_check = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_MAKE_DEFAULT).c_str());
  FormTableSetWidget(label_table, default_check, 0, 1, 1, true);

  GtkWidget* name_cc_table = InitFormTable(2, 6);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), name_cc_table);

  FormTableAddExpandedEntry(name_cc_table, 0, 0, 3,
                            IDS_AUTOFILL_DIALOG_NAME_ON_CARD);
  FormTableAddExpandedEntry(name_cc_table, 1, 0, 3,
                            IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER);
  FormTableAddSizedEntry(name_cc_table, 1, 3, 2, 0);
  FormTableAddSizedEntry(name_cc_table, 1, 4, 4, 0);
  FormTableAddSizedEntry(name_cc_table, 1, 5, 5, IDS_AUTOFILL_DIALOG_CVC);

  FormTableSetLabel(name_cc_table, 1, 3, 2,
                    IDS_AUTOFILL_DIALOG_EXPIRATION_DATE);

  gtk_table_set_col_spacing(GTK_TABLE(name_cc_table), 3, 2);

  GtkWidget* addresses_table = InitFormTable(2, 5);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), addresses_table);

  FormTableSetLabel(addresses_table, 0, 0, 3,
                    IDS_AUTOFILL_DIALOG_BILLING_ADDRESS);

  GtkWidget* billing = gtk_combo_box_new_text();
  std::string combo_text = l10n_util::GetStringUTF8(
      IDS_AUTOFILL_DIALOG_CHOOSE_EXISTING_ADDRESS);
  gtk_combo_box_append_text(GTK_COMBO_BOX(billing), combo_text.c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(billing), 0);
  FormTableSetWidget(addresses_table, billing, 0, 0, 2, false);

  FormTableSetLabel(addresses_table, 1, 0, 3,
                    IDS_AUTOFILL_DIALOG_SHIPPING_ADDRESS);

  GtkWidget* shipping = gtk_combo_box_new_text();
  combo_text = l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_SAME_AS_BILLING);
  gtk_combo_box_append_text(GTK_COMBO_BOX(shipping), combo_text.c_str());
  gtk_combo_box_set_active(GTK_COMBO_BOX(shipping), 0);
  FormTableSetWidget(addresses_table, shipping, 1, 0, 2, false);

  GtkWidget* phone_table = InitFormTable(1, 4);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), phone_table);

  FormTableAddSizedEntry(phone_table, 0, 0, 4, IDS_AUTOFILL_DIALOG_PHONE);
  FormTableAddSizedEntry(phone_table, 0, 1, 4, 0);
  FormTableAddEntry(phone_table, 0, 2, 2, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  GtkWidget* alignment = gtk_alignment_new(0, 0, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), button);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), alignment);

  return credit_card;
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowAutoFillDialog(std::vector<AutoFillProfile>* profiles,
                        std::vector<FormGroup>* credit_cards) {
  if (!dialog) {
    dialog = new AutoFillDialog(profiles, credit_cards);
  }
  dialog->Show();
}
