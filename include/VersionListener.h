#include <JSON/Listener.h>
namespace JSON
{
    class VersionListener : public Listener
    {
    public:
        VersionListener()
        {
            version = 0;
            gotVersion = false;
        }

        ~VersionListener()
        {};

        bool startElement(const Element& element) override
        {
            debug_i("startElement: %s", element.key);
            if (std::string(element.key) == "version")
            {
                version = static_cast<uint8_t>(std::stoi(element.value));
                gotVersion = true;
                return false;
            }
            return true;
        }

        bool endElement(const Element& element) override
        {
            if (std::string(element.key) == "version")
            {
                return false;
            }
            return true;
        }

        bool hasVersion() const
        {
            return gotVersion;
        }

        uint8_t getVersion() const
        {
            return version;
        }

    private:
        uint8_t version;
        bool gotVersion;
    };
}//namespace JSON