#include "net/NetworkError.h"

std::optional<NetworkError> networkErrorFromStatusCode(int statusCode)
{
    if (statusCode >= 200 && statusCode < 300)
        return std::nullopt;

    switch (statusCode) {
    case 401:
    case 403:
        return NetworkError::Unauthorized;
    case 409:
        return NetworkError::Conflict;
    case 429:
        return NetworkError::RateLimited;
    case 503:
        return NetworkError::ServiceUnavailable;
    default:
        return NetworkError::Server;
    }
}
