
#include "EfficientSAM.hpp"

#include "utils/Deep_Learning/torch_helpers.hpp"
#include <torch/torch.h>

namespace dl {


EfficientSAM::EfficientSAM()
{
    _module_path = "resources/efficient_sam_vitt_torchscript.pt";
}

EfficientSAM::~EfficientSAM()
{

}

void EfficientSAM::load_model()
{
    if (!_module) {
        _module = dl::load_torchscript_model(_module_path, device);
    }
}

std::vector<uint8_t> EfficientSAM::process_frame(
    std::vector<uint8_t> const & image,
    int const height,
    int const width,
    int const x,
    int const y)
{
    const int channels = 3;

    auto image_tensor = dl::create_tensor_from_gray8(image, height, width);
    if (channels > 1) {
        image_tensor = image_tensor.repeat({1, channels, 1, 1});
    }

    auto input_points = torch::tensor({{{{x, y}}}}, torch::kInt32);
    auto input_labels = torch::tensor({{{1}}}, torch::kInt32);

    auto output = _module->forward({image_tensor, input_points, input_labels}).toTuple();

    auto predicted_logits = output->elements()[0].toTensor();
    auto predicted_iou = output->elements()[1].toTensor();

    auto sorted_ids = std::get<1>(predicted_iou.sort(-1, true));
    predicted_iou = predicted_iou.gather(2, sorted_ids);
    predicted_logits = predicted_logits.gather(2, sorted_ids.unsqueeze(-1).unsqueeze(-1));

    auto mask = torch::ge(predicted_logits[0][0][0], 0).to(torch::kUInt8).cpu();

    std::vector<uint8_t> mask_vector(mask.data_ptr<uint8_t>(), mask.data_ptr<uint8_t>() + mask.numel());

    return mask_vector;
}


}
