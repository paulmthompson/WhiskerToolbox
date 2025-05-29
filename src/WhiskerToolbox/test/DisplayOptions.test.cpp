TEST_CASE("MaskDisplayOptions", "[DisplayOptions][Mask]") {
    SECTION("Default values") {
        MaskDisplayOptions opts;
        
        REQUIRE(opts.hex_color == DefaultDisplayValues::COLOR);
        REQUIRE(opts.alpha == DefaultDisplayValues::ALPHA);
        REQUIRE(opts.is_visible == DefaultDisplayValues::VISIBLE);
        REQUIRE(opts.show_bounding_box == false);
        REQUIRE(opts.show_outline == false);
    }
    
    SECTION("Configurable values") {
        MaskDisplayOptions opts;
        
        opts.hex_color = "#ff5500";
        opts.alpha = 0.7f;
        opts.is_visible = true;
        opts.show_bounding_box = true;
        opts.show_outline = true;
        
        REQUIRE(opts.hex_color == "#ff5500");
        REQUIRE(opts.alpha == 0.7f);
        REQUIRE(opts.is_visible == true);
        REQUIRE(opts.show_bounding_box == true);
        REQUIRE(opts.show_outline == true);
    }
} 