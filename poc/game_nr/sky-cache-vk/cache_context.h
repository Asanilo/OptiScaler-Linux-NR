#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Owned standalone Vulkan context, no NR/core/NVAPI/DXGI or external memory/FD path.
#define CI(n) instance_fn<PFN_##n>(#n)
#define CD(n) device_fn<PFN_##n>(#n)
struct CacheContext {
 HMODULE loader{};PFN_vkGetInstanceProcAddr gipa{};PFN_vkGetDeviceProcAddr gdpa{};
 VkInstance instance{};VkPhysicalDevice physical{};VkDevice device{};VkQueue queue{};uint32_t family=0;
 VkPhysicalDeviceMemoryProperties memory{};VkPhysicalDeviceIDProperties ids{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};VkPhysicalDeviceProperties properties{};
 VkCommandPool pool{};VkCommandBuffer cmd{};
 template<class T>T instance_fn(const char*n){auto p=reinterpret_cast<T>(gipa(instance,n));require(p,n,3);return p;}
 template<class T>T device_fn(const char*n){auto p=reinterpret_cast<T>(gdpa(device,n));require(p,n,3);return p;}
 void init(){
  loader=LoadLibraryA("vulkan-1.dll");require(loader,"Vulkan loader",3);gipa=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader,"vkGetInstanceProcAddr"));require(gipa,"gipa",3);
  VkApplicationInfo a{VK_STRUCTURE_TYPE_APPLICATION_INFO};a.apiVersion=VK_API_VERSION_1_2;a.pApplicationName="Owned Sky cache fixture NOT game";VkInstanceCreateInfo i{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};i.pApplicationInfo=&a;api(CI(vkCreateInstance)(&i,nullptr,&instance),"CacheInstance");
  uint32_t n=0;api(CI(vkEnumeratePhysicalDevices)(instance,&n,nullptr),"DevicesCount");std::vector<VkPhysicalDevice> ps(n);api(CI(vkEnumeratePhysicalDevices)(instance,&n,ps.data()),"Devices");
  for(auto p:ps){VkPhysicalDeviceProperties2 pr{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};pr.pNext=&ids;CI(vkGetPhysicalDeviceProperties2)(p,&pr);if(pr.properties.vendorID==0x10de){physical=p;properties=pr.properties;break;}}
  require(physical&&hex(ids.deviceUUID)=="8734fac9db30c279a340fe117ace6365","Pinned NVIDIA device UUID",3);CI(vkGetPhysicalDeviceMemoryProperties)(physical,&memory);
  const auto&l=properties.limits;require(l.maxPerStageDescriptorStorageImages>=7&&l.maxDescriptorSetStorageImages>=7&&l.maxPerStageDescriptorSampledImages>=12&&l.maxDescriptorSetSampledImages>=12&&l.maxUniformBufferRange>=256,"Cache descriptor limits",3);
  for(auto f:{VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R32_SFLOAT,VK_FORMAT_R32G32_SFLOAT,VK_FORMAT_R32_UINT}){VkFormatProperties fp{};CI(vkGetPhysicalDeviceFormatProperties)(physical,f,&fp);VkFormatFeatureFlags flags=VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT;require((fp.optimalTilingFeatures&flags)==flags,"Owned cache format sampled/storage/transfer",3);if(f==VK_FORMAT_R32G32B32A32_SFLOAT)require(fp.optimalTilingFeatures&VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,"Cache RGBA32F linear sampler",3);}
  CI(vkGetPhysicalDeviceQueueFamilyProperties)(physical,&n,nullptr);std::vector<VkQueueFamilyProperties> qs(n);CI(vkGetPhysicalDeviceQueueFamilyProperties)(physical,&n,qs.data());bool found=false;for(uint32_t j=0;j<n;j++)if(qs[j].queueCount&&(qs[j].queueFlags&VK_QUEUE_COMPUTE_BIT)){family=j;found=true;break;}require(found,"Compute queue",3);
  float priority=.5;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qi;api(CI(vkCreateDevice)(physical,&dc,nullptr,&device),"CacheDevice");gdpa=CI(vkGetDeviceProcAddr);CD(vkGetDeviceQueue)(device,family,0,&queue);
  VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=family;cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;api(CD(vkCreateCommandPool)(device,&cp,nullptr,&pool),"CachePool");VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;api(CD(vkAllocateCommandBuffers)(device,&ca,&cmd),"CacheCommand");
 }
 void begin(bool actual_safe){require(actual_safe,"Owned slot reuse needs actual completed GPU",3);api(CD(vkResetCommandBuffer)(cmd,0),"ResetCommand");VkCommandBufferBeginInfo ci{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};ci.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;api(CD(vkBeginCommandBuffer)(cmd,&ci),"BeginCommand");}
 void shutdown(){if(pool)CD(vkDestroyCommandPool)(device,pool,nullptr);pool={};if(device)CD(vkDestroyDevice)(device,nullptr);device={};if(instance)CI(vkDestroyInstance)(instance,nullptr);instance={};if(loader)FreeLibrary(loader);loader={};}
};
