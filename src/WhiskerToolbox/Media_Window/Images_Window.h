#ifndef IMAGES_WINDOW_H
#define IMAGES_WINDOW_H

#include "Media_Window.h"
#include "Media_Data.h"

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

private:
    std::vector<std::filesystem::path> _image_paths;
};

class Images_Window : public Media_Window
{
    Q_OBJECT
public:
    Images_Window(QObject *parent = 0);

private:

};
#endif // IMAGES_WINDOW_H
