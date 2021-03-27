#include <Arduino.h>
#include <Schedule.h>

class SerialReq
{
    static const size_t _bufferSize = 128;
    char _buffer[_bufferSize];
    const char *_prefix;
    const char *_suffix;
    size_t _prefixLen;
    size_t _suffixLen;
    size_t _offset;
    bool _sendPrefix;
    bool _sendSuffix;
    bool _busy;
    bool _startOfLine;
    bool _isLastLine;
    std::function<void()> _sendSerial;

public:
    SerialReq(const char *prefix, const char *suffix)
        : _prefix(prefix), _suffix(suffix), _busy(false), _startOfLine(false), _isLastLine(false)
    {
        _prefixLen = strlen(_prefix);
        _suffixLen = strlen(_suffix);
        _sendSerial = [this]() {
            // Flush any extra read data before sending command
            while (Serial.available())
            {
                Serial.read();
            }
            Serial.write(_buffer);
            Serial.write("\n");
        };
    }
    void sendRequest(const char *req)
    {
        if (strlen(req) >= _bufferSize)
        {
            strcpy(_buffer, "{\"error\":\"Req too long\"");
        }
        else
        {
            strncpy(_buffer, req, sizeof(_buffer));
            _offset = 0;
            _sendPrefix = (_prefix != nullptr);
            _sendSuffix = (_suffix != nullptr);
            _busy = true;
            _startOfLine = true;
            _isLastLine = false;
            schedule_function(_sendSerial);
        }
    }
    bool isIdle()
    {
        return !_busy && !_sendSuffix;
    }
    const char *getResponse(size_t *numBytes)
    {
        size_t maxLen = *numBytes;
        size_t offset = _offset;

        if (_sendPrefix)
        {
            size_t toSend = _prefixLen - offset;
            if (toSend <= maxLen)
            {
                // Can send complete/remainder
                *numBytes = toSend;
                _sendPrefix = false;
                _offset = 0;
            }
            else
            {
                // Not enough room, send partial
                *numBytes = maxLen;
                _offset += maxLen;
            }
            return &_prefix[offset];
        }
        else if (!_busy)
        {
            if (_sendSuffix)
            {
                size_t toSend = _suffixLen - offset;
                if (toSend <= maxLen)
                {
                    // Can send complete/remainder
                    *numBytes = toSend;
                    _sendSuffix = false;
                    _offset = 0;
                }
                else
                {
                    // Not enough room, send partial
                    *numBytes = maxLen;
                    _offset += maxLen;
                }
                return &_suffix[offset];
            }
            else
            {
                *numBytes = 0;
                return nullptr;
            }
        }

        offset = 0;
        while (Serial.available() && (offset < _bufferSize) && (offset < maxLen))
        {
            int b = Serial.read();
            _buffer[offset] = static_cast<char>(b);
            offset++;
            if (_startOfLine && b == '{') {
                _isLastLine = true;
            }
            if (b == '\r' || b == '\n' || b == '\0')
            {
                _startOfLine = true;
                if (_isLastLine) {
                    _busy = false;
                }
                break;
            }
        }
        *numBytes = offset;
        return &_buffer[0];
    }
};