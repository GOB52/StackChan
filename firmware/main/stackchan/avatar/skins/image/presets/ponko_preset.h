// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include "../image_avatar.h"
#include <memory>

namespace stackchan::avatar::image {

std::unique_ptr<ImageAvatar> make_ponko_avatar();

}  // namespace stackchan::avatar::image
