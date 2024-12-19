#ifndef JSON_FEATURES_H_INCLUDED
#define JSON_FEATURES_H_INCLUDED

#if !defined(JSON_IS_AMALGAMATION)
#include "forwards.h"
#endif

#pragma pack(push)
#pragma pack()

namespace Json {
/*!
\class Features
\brief Configures parsing and generation options for JSON processing.

Provides a set of boolean flags to control various aspects of JSON parsing and generation.
Allows customization of behavior such as comment handling, strict root element enforcement, treatment of dropped null placeholders, and acceptance of numeric keys.
Offers static methods to create instances with predefined configurations, enabling developers to tailor JSON processing to specific requirements.
*/
class JSON_API Features {
public:
  /*!
  \brief Creates a Features object with all options enabled.
  
  Generates and returns a Features instance with all available JSON parsing and generation options set to their permissive values.
  This provides the most flexible configuration for JSON processing.
  
  \return A Features object with all options enabled.
  */
  static Features all();

  /*!
  \brief Creates a Features object with strict parsing settings.
  
  Configures a Features object with strict JSON parsing rules.
  Disables comments, enforces a strict root element, disallows dropped null placeholders, and prohibits numeric keys.
  
  \return A Features object configured for strict JSON parsing.
  */
  static Features strictMode();

  Features();

  bool allowComments_{true};

  bool strictRoot_{false};

  bool allowDroppedNullPlaceholders_{false};

  bool allowNumericKeys_{false};
};

} // namespace Json

#pragma pack(pop)

#endif
