#ifndef IMAGES_WINDOW_H
#define IMAGES_WINDOW_H

#include "Media_Window.h"

#include <string>
#include <vector>
#include <filesystem>

class Images_Window : public Media_Window
{
    Q_OBJECT
public:
    Images_Window(QObject *parent = 0);

private:
    int doLoadMedia(std::string name) override;
    int doLoadFrame(int frame_id) override;
    int doFindNearestSnapFrame(int frame_id) const override;

    std::vector<std::filesystem::path> image_paths;

};
#endif // IMAGES_WINDOW_H
