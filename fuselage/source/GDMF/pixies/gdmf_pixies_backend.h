#ifndef GDMF_PIXIES_BACKEND_H
#define GDMF_PIXIES_BACKEND_H

// Private lifecycle services owned by the Pixies backend.
// Calls are serialized with frame preparation and command submission.
#include <stdbool.h>

// True only when resource ownership has transferred or no resources remain,
// and recorded GPU references cannot affect a replacement pixie. Logical release
// cancels this ID's unrecorded requests after a successful handoff.
// False retains backend ownership and prevents logical-state destruction.
bool gdmf_pixies_backend_release(int id);

// Requires excluded producers, completed submissions, and discarded recorded
// references. True releases shared backend resources; logical shutdown then
// frees pending request storage.
// Refuses cleanup while any logical Pixie IDs remain initialized.
// False retains any resources that cannot yet be released. This does not wait.
bool gdmf_pixies_backend_shutdown(void);
bool gdmf_pixies_backend_linear_filter_available(void);

#endif
