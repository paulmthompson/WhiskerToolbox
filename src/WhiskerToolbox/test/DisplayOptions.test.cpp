TEST_CASE("MaskDisplayOptions", "[DisplayOptions][Mask]") {
    SECTION("Default values") {
        MaskDisplayOptions opts;
        
        REQUIRE(opts.style.hex_color == DefaultDisplayValues::COLOR);
        REQUIRE(opts.style.alpha == DefaultDisplayValues::ALPHA);
        REQUIRE(opts.style.is_visible == DefaultDisplayValues::VISIBLE);
        REQUIRE(opts.show_bounding_box == false);
        REQUIRE(opts.show_outline == false);
    }
    
    SECTION("Configurable values") {
        MaskDisplayOptions opts;
        
        opts.style.hex_color = "#ff5500";
        opts.style.alpha = 0.7f;
        opts.style.is_visible = true;
        opts.show_bounding_box = true;
        opts.show_outline = true;
        
        REQUIRE(opts.style.hex_color == "#ff5500");
        REQUIRE(opts.style.alpha == 0.7f);
        REQUIRE(opts.style.is_visible == true);
        REQUIRE(opts.show_bounding_box == true);
        REQUIRE(opts.show_outline == true);
    }
} 