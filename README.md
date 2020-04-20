# Back to the Future

OS Patch Level changer for boot and recovery partitions. Flash in TWRP. You can set desired date in filename. Date format: YYYY-MM. For example: backtothefuture-2021-12.zip If there will be no date in the filename, max possible date 2127-12 will be used. If you select a wrong date, then don't reboot, rename the zipfile and flash it again.

Downloads: https://github.com/CruelKernel/backtothefuture/releases

If someone wants to know more about why it's not possible to rollback to normal date, here is the official documentation: https://source.android.com/security/keystore/version-binding#hal-changes. Short answer: because all your security keys are cryptographically updated with new os_patch_level date once you boot with "new date" kernel.

So, once you installed a kernel with os_patch_level in future you are doomed to use greater or equal os_patch_level date for all your next kernels. However, full wipe will help.
