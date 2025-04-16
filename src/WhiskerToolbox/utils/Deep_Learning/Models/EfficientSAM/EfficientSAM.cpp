
#include "EfficientSAM.hpp"

#include "utils/Deep_Learning/torch_helpers.hpp"
#include <torch/torch.h>

namespace dl {


EfficientSAM::EfficientSAM()
{
    _module_path = "resources/efficient_sam_vitt_torchscript.pt";
}

EfficientSAM::~EfficientSAM()= default;

void EfficientSAM::load_model()
{
    if (!_module) {
        _module = dl::load_torchscript_model(_module_path, device);
    }
}

std::vector<uint8_t> EfficientSAM::process_frame(
    std::vector<uint8_t> const & image,
    ImageSize const image_size,
    int const x,
    int const y)
{
    const int channels = 3;

    auto image_tensor = dl::create_tensor_from_gray8(image, image_size);
    if (channels > 1) {
        image_tensor = image_tensor.repeat({1, channels, 1, 1});
    }

    image_tensor = image_tensor.to(torch::kFloat32).div(255).to(device);

    auto input_points = torch::tensor({{{{x, y}}}}, torch::kInt32);
    auto input_labels = torch::tensor({{{1}}}, torch::kInt32);

    auto output = _module->forward({image_tensor, input_points, input_labels}).toTuple();

    auto predicted_logits = output->elements()[0].toTensor();
    auto predicted_iou = output->elements()[1].toTensor();

    auto sorted_ids = std::get<1>(predicted_iou.sort(-1, true));

    predicted_iou = predicted_iou.take_along_dim(sorted_ids, 2);

    sorted_ids = sorted_ids.unsqueeze(-1);
    sorted_ids = sorted_ids.unsqueeze(-1);

    predicted_logits = predicted_logits.take_along_dim(sorted_ids, 2);

    auto sub_mask = predicted_logits.index({
        0,
        0,
        0,
        torch::indexing::Slice(),
        torch::indexing::Slice()
    });

    auto mask = torch::ge(sub_mask, 0).to(torch::kUInt8).cpu();

    std::vector<uint8_t> mask_vector(mask.data_ptr<uint8_t>(), mask.data_ptr<uint8_t>() + mask.numel());

    return mask_vector;
}


}
