#ifndef IMAGES_WINDOW_H
#define IMAGES_WINDOW_H

#include "Media_Window.h"

#include <string>
#include <vector>
#include <filesystem>

class ImageData : public MediaData {
public:
    ImageData();
    int LoadMedia(std::string name) override;
    void LoadFrame(int frame_id) override;
    std::string GetFrameID(int frame_id) override;

protected:
    std::vector<std::filesystem::path> image_paths;
};

class Images_Window : public Media_Window
{
    Q_OBJECT
public:
    Images_Window(QObject *parent = 0);

private:
    //int doLoadMedia(std::string name) override;
    //void doLoadFrame(int frame_id) override;
    //std::string doGetFrameID(int frame_id) override;

    //std::vector<std::filesystem::path> image_paths;

};
#endif // IMAGES_WINDOW_H
