#ifndef EFFICIENTVIT_HPP
#define EFFICIENTVIT_HPP


#include <torch/torch.h>
#include <cmath>
#include <vector>
#include <string>

struct Conv2dSameImpl : torch::nn::Conv2dImpl {
    using torch::nn::Conv2dImpl::Conv2dImpl;

    int calc_same_pad(int i, int k, int s, int d) {
        return std::max((int(std::ceil(float(i) / s)) - 1) * s + (k - 1) * d + 1 - i, 0);
    }

    torch::Tensor forward(torch::Tensor x) {
        auto ih = x.size(-2);
        auto iw = x.size(-1);

        auto pad_h = calc_same_pad(ih, this->options.kernel_size()->at(0), this->options.stride()->at(0), this->options.dilation()->at(0));
        auto pad_w = calc_same_pad(iw, this->options.kernel_size()->at(1), this->options.stride()->at(1), this->options.dilation()->at(1));

        if (pad_h > 0 || pad_w > 0) {
            x = torch::nn::functional::pad(x, torch::nn::functional::PadFuncOptions({pad_w / 2, pad_w - pad_w / 2, pad_h / 2, pad_h - pad_h / 2}));
        }
        return this->_conv_forward(x, this->weight);
    }
};
TORCH_MODULE(Conv2dSame);

struct MBConvImpl : torch::nn::Module {
    bool shortcut, is_fused, use_norm, use_output_norm, anti_aliasing;
    int strides, expansion;
    float drop_rate;
    Conv2dSame expand_conv{nullptr}, dw_conv{nullptr}, pw_conv{nullptr};
    torch::nn::BatchNorm2d expand_bn{nullptr}, dw_bn{nullptr}, pw_bn{nullptr};
    torch::nn::Dropout dropout{nullptr};
    torch::nn::ModuleHolder<torch::nn::ReLUImpl> activation;

    MBConvImpl(
        int input_channel,
        int output_channel,
        bool shortcut=true,
        int strides=1,
        int expansion=4,
        bool is_fused=false,
        bool use_bias=true,
        bool use_norm=false,
        bool use_output_norm=false,
        float drop_rate=0,
        bool anti_aliasing=false)
        : shortcut(shortcut), strides(strides), expansion(expansion), is_fused(is_fused), use_norm(use_norm),
        use_output_norm(use_output_norm), drop_rate(drop_rate), anti_aliasing(anti_aliasing) {

        if (is_fused) {
            if (anti_aliasing && strides > 1) {
                expand_conv = Conv2dSame(torch::nn::Conv2dOptions(input_channel, int(input_channel * expansion), 3).stride(1).bias(true));
                // Blur2D implementation needed
            } else {
                expand_conv = Conv2dSame(torch::nn::Conv2dOptions(input_channel, int(input_channel * expansion), 3).stride(strides).bias(true));
            }
            if (use_norm) {
                expand_bn = torch::nn::BatchNorm2d(int(input_channel * expansion));
            }
        } else if (expansion > 1) {
            expand_conv = Conv2dSame(torch::nn::Conv2dOptions(input_channel, int(input_channel * expansion), 1).stride(1).bias(true));
            if (use_norm) {
                expand_bn = torch::nn::BatchNorm2d(int(input_channel * expansion));
            }
        }

        if (!is_fused) {
            if (anti_aliasing && strides > 1) {
                dw_conv = Conv2dSame(torch::nn::Conv2dOptions(int(input_channel * expansion), int(input_channel * expansion), 3).stride(1).groups(int(input_channel * expansion)).bias(true));
                // Blur2D implementation needed
            } else {
                dw_conv = Conv2dSame(torch::nn::Conv2dOptions(int(input_channel * expansion), int(input_channel * expansion), 3).stride(strides).groups(int(input_channel * expansion)).bias(true));
            }
            if (use_norm) {
                dw_bn = torch::nn::BatchNorm2d(int(input_channel * expansion));
            }
        }

        int pw_kernel_size = (is_fused && expansion == 1) ? 3 : 1;
        pw_conv = Conv2dSame(torch::nn::Conv2dOptions(int(input_channel * expansion), output_channel, pw_kernel_size).stride(1).bias(true));
        if (use_output_norm) {
            pw_bn = torch::nn::BatchNorm2d(output_channel);
        }

        dropout = torch::nn::Dropout(torch::nn::DropoutOptions().p(drop_rate));

        register_module("expand_conv", expand_conv);
        register_module("dw_conv", dw_conv);
        register_module("pw_conv", pw_conv);
        if (use_norm) {
            register_module("expand_bn", expand_bn);
            register_module("dw_bn", dw_bn);
            register_module("pw_bn", pw_bn);
        }
        register_module("dropout", dropout);
    }

    torch::Tensor forward(torch::Tensor input) {
        auto x = input;
        if (expand_conv) {
            x = expand_conv->forward(x);
            if (use_norm) {
                x = expand_bn->forward(x);
            }
            x = activation->forward(x);
        }

        if (!is_fused) {
            x = dw_conv->forward(x);
            if (use_norm) {
                x = dw_bn->forward(x);
            }
            x = activation->forward(x);
        }

        x = pw_conv->forward(x);
        if (use_output_norm) {
            x = pw_bn->forward(x);
        }
        x = dropout->forward(x);

        if (shortcut) {
            return x + input;
        } else {
            return x;
        }
    }
};
TORCH_MODULE(MBConv);

struct LiteMHSAImpl : torch::nn::Module {
    int num_heads, key_dim, emb_dim;
    bool use_norm;
    torch::nn::ModuleHolder<torch::nn::ReLUImpl> activation;
    Conv2dSame qkv_conv{nullptr}, qkv_dw_conv{nullptr}, qkv_pw_conv{nullptr}, out_conv{nullptr};
    torch::nn::BatchNorm2d out_bn{nullptr};

    LiteMHSAImpl(
        int input_channel,
        int num_heads=8,
        int key_dim=16,
        int sr_ratio=5,
        bool qkv_bias=true,
        int out_shape=-1,
        bool out_bias=false,
        bool use_norm=true,
        float dropout=0) :
        num_heads(num_heads),
        key_dim(key_dim),
        use_norm(use_norm) {

        emb_dim = num_heads * key_dim;
        if (out_shape == -1) {
            out_shape = input_channel;
        }

        qkv_conv = Conv2dSame(torch::nn::Conv2dOptions(input_channel, emb_dim * 3, 1).bias(qkv_bias));
        qkv_dw_conv = Conv2dSame(torch::nn::Conv2dOptions(emb_dim * 3, emb_dim * 3, sr_ratio).groups(emb_dim * 3).bias(qkv_bias));
        qkv_pw_conv = Conv2dSame(torch::nn::Conv2dOptions(emb_dim * 3, emb_dim * 3, 1).groups(3 * num_heads).bias(qkv_bias));
        out_conv = Conv2dSame(torch::nn::Conv2dOptions(emb_dim * 2, out_shape, 1).bias(out_bias));
        if (use_norm) {
            out_bn = torch::nn::BatchNorm2d(out_shape);
        }

        register_module("qkv_conv", qkv_conv);
        register_module("qkv_dw_conv", qkv_dw_conv);
        register_module("qkv_pw_conv", qkv_pw_conv);
        register_module("out_conv", out_conv);
        if (use_norm) {
            register_module("out_bn", out_bn);
        }
        register_module("activation", activation);
    }

    torch::Tensor forward(torch::Tensor input) {
        auto batch_size = input.size(0);
        auto height = input.size(2);
        auto width = input.size(3);

        auto qkv = qkv_conv->forward(input);
        auto sr_qkv = qkv_dw_conv->forward(qkv);
        sr_qkv = qkv_pw_conv->forward(sr_qkv);
        qkv = torch::cat({qkv, sr_qkv}, 1);

        qkv = qkv.view({batch_size, emb_dim * 6 / (3 * key_dim), 3 * key_dim, height * width});
        auto query = qkv.slice(2, 0, key_dim);
        auto key = qkv.slice(2, key_dim, 2 * key_dim);
        auto value = qkv.slice(2, 2 * key_dim, 3 * key_dim);

        query = activation->forward(query);
        key = activation->forward(key);

        auto query_key = torch::matmul(query.transpose(-2, -1), key);
        auto scale = torch::sum(query_key, -1, true);
        auto attention_output = torch::matmul(query_key, value.transpose(-2, -1)) / (scale + 1e-7);

        auto output = attention_output.permute({0, 1, 3, 2}).contiguous().view({batch_size, -1, height, width});
        output = out_conv->forward(output);
        if (use_norm) {
            output = out_bn->forward(output);
        }
        output = output + input;
        return output;
    }
};
TORCH_MODULE(LiteMHSA);

struct EfficientViT_BImpl : torch::nn::Module {
    std::vector<int> num_blocks, out_channels, expansions, output_filters;
    int stem_width, head_dimension;
    bool use_norm, anti_aliasing;
    float drop_connect_rate, dropout;
    Conv2dSame stem_conv{nullptr};
    torch::nn::BatchNorm2d stem_bn{nullptr};
    torch::nn::ModuleHolder<torch::nn::ReLUImpl> stem_activation;
    MBConv stem_mb_conv{nullptr};
    torch::nn::Sequential blocks{nullptr};
    Conv2dSame features_conv{nullptr};
    torch::nn::BatchNorm2d features_bn{nullptr};
    std::vector<std::string> block_types;
    std::vector<bool> is_fused;

    EfficientViT_BImpl(
        std::vector<int> num_blocks,
        std::vector<int> out_channels,
        int stem_width=8,
        std::vector<std::string> block_types={"conv", "conv", "transform", "transform"},
        std::vector<int> expansions={4},
        std::vector<bool> is_fused={false, false, false, false},
        int head_dimension=16,
        std::vector<int> output_filters={1024, 1280},
        std::vector<int> input_shape={3, 224, 224},
        float drop_connect_rate=0,
        float dropout=0,
        bool use_norm=true,
        bool anti_aliasing=false) :
        num_blocks(num_blocks),
        out_channels(out_channels),
        output_filters(output_filters),
        stem_width(stem_width),
        head_dimension(head_dimension),
        use_norm(use_norm),
        anti_aliasing(anti_aliasing),
        drop_connect_rate(drop_connect_rate),
        dropout(dropout),
        block_types(block_types)
    {

        stem_conv = Conv2dSame(torch::nn::Conv2dOptions(input_shape[0], stem_width, 3).stride(2));
        if (use_norm) {
            stem_bn = torch::nn::BatchNorm2d(stem_width);
        }
        stem_mb_conv = MBConv(stem_width, stem_width, true, 1, 1, is_fused[0], true, use_norm, use_norm, 0, anti_aliasing);

        blocks = _make_blocks(stem_width);

        if (output_filters[0] > 0) {
            features_conv = Conv2dSame(torch::nn::Conv2dOptions(out_channels.back(), output_filters[0], 1).stride(1));
            if (use_norm) {
                features_bn = torch::nn::BatchNorm2d(output_filters[0]);
            }
        }

        register_module("stem_conv", stem_conv);
        register_module("stem_bn", stem_bn);
        register_module("stem_activation", stem_activation);
        register_module("stem_mb_conv", stem_mb_conv);
        register_module("blocks", blocks);
        register_module("features_conv", features_conv);
        register_module("features_bn", features_bn);
    }

    torch::nn::Sequential _make_blocks(int block_input_channels) {
        torch::nn::Sequential blocks;
        int total_blocks = std::accumulate(num_blocks.begin(), num_blocks.end(), 0);
        int global_block_id = 0;

        for (size_t stack_id = 0; stack_id < num_blocks.size(); ++stack_id) {
            bool is_conv_block = block_types[stack_id][0] == 'c';
            int cur_expansions = expansions[stack_id];

            bool block_use_bias = stack_id >= 2;
            bool block_use_norm = stack_id < 2;

            if (!use_norm) {
                block_use_norm = false;
                block_use_bias = true;
            }

            bool cur_is_fused = is_fused[stack_id];
            for (int block_id = 0; block_id < num_blocks[stack_id]; ++block_id) {
                std::string name = "stack_" + std::to_string(stack_id + 1) + "_block_" + std::to_string(block_id + 1) + "_";
                int stride = block_id == 0 ? 2 : 1;
                bool shortcut = block_id != 0;
                int cur_expansion = cur_expansions;

                float block_drop_rate = drop_connect_rate * global_block_id / total_blocks;

                if (is_conv_block || block_id == 0) {
                    std::string cur_name = stride > 1 ? name + "downsample_" : name;
                    blocks->push_back(MBConv(block_input_channels, out_channels[stack_id], shortcut, stride, cur_expansion, cur_is_fused, block_use_bias, block_use_norm, use_norm, block_drop_rate, anti_aliasing));
                } else {
                    int num_heads = out_channels[stack_id] / head_dimension;
                    blocks->push_back(LiteMHSA(block_input_channels, num_heads, head_dimension, 5, true, -1, false, use_norm, 0));
                    blocks->push_back(MBConv(block_input_channels, out_channels[stack_id], shortcut, stride, cur_expansion, cur_is_fused, block_use_bias, block_use_norm, use_norm, block_drop_rate, anti_aliasing));
                }

                block_input_channels = out_channels[stack_id];
                global_block_id++;
            }
        }

        return blocks;
    }

    torch::Tensor forward(torch::Tensor x) {
        x = stem_conv->forward(x);
        if (use_norm) {
            x = stem_bn->forward(x);
        }
        x = stem_activation->forward(x);
        x = stem_mb_conv->forward(x);
        x = blocks->forward(x);

        if (output_filters[0] > 0) {
            x = features_conv->forward(x);
            if (use_norm) {
                x = features_bn->forward(x);
            }
            x = stem_activation->forward(x);
        }

        return x;
    }
};
TORCH_MODULE(EfficientViT_B);

struct Blur2DImpl : torch::nn::Module {
    int kernel_size, stride;
    std::string padding;
    torch::Tensor kernel;

    Blur2DImpl(int kernel_size=5, int stride=2, std::string padding="same")
        : kernel_size(kernel_size), stride(stride), padding(padding) {

        kernel = torch::ones({1, 1, kernel_size, kernel_size}) / (kernel_size * kernel_size);
    }

    torch::Tensor forward(torch::Tensor x) {
        auto batch_size = x.size(0);
        auto channels = x.size(1);
        auto height = x.size(2);
        auto width = x.size(3);

        auto kernel_repeated = kernel.repeat({channels, 1, 1, 1}).to(x.device());

        int pad = (padding == "same") ? kernel_size / 2 : 0;

        auto blurred = torch::nn::functional::conv2d(
            x,
            kernel_repeated,
            torch::nn::functional::Conv2dFuncOptions().stride(stride).padding(pad));
        return blurred;
    }
};
TORCH_MODULE(Blur2D);

#endif // EFFICIENTVIT_HPP
